/*

  Copyright (c) 2026 Azzam Wildan Maulana

  This file is part of cangaroo.

  cangaroo is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  cangaroo is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with cangaroo.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "EcanInterface.h"
#include "EcanDriver.h"

#include <core/Backend.h>
#include <core/MeasurementInterface.h>
#include <core/CanMessage.h>

#include <sys/time.h>

#include <QtNetwork/QTcpSocket>
#include <QThread>

// Ebyte ECAN-E01 fixed 13-byte frame:
//   byte 0   : info  (bit7 IDE/extended, bit6 RTR, low nibble = DLC 0..8)
//   byte 1-4 : CAN ID, big-endian uint32, right-justified
//   byte 5-12: 8 data bytes (zero-padded when DLC < 8)
#define ECAN_FRAME_LEN  13
#define ECAN_INFO_EXT   0x80
#define ECAN_INFO_RTR   0x40

EcanInterface::EcanInterface(EcanDriver *driver, int index, QString name)
  : CanInterface((CanDriver *)driver),
    _idx(index),
    _isOpen(false),
    _name(name),
    _host("192.168.9.20"),
    _port(8882),
    _socket(NULL),
    _rx_count(0),
    _tx_count(0),
    _rx_errors(0),
    _tx_errors(0)
{
    _settings.setBitrate(1000000);
    _settings.setSamplePoint(875);
}

EcanInterface::~EcanInterface() {
    if (_socket != NULL) {
        delete _socket;
        _socket = NULL;
    }
}

QString EcanInterface::getName() const {
    return _name;
}

void EcanInterface::setName(QString name) {
    _name = name;
}

QString EcanInterface::getDetailsStr() const {
    return QString("Ebyte ECAN-E01 TCP gateway (%1:%2)").arg(_host).arg(_port);
}

bool EcanInterface::needsHostConfig() const {
    return true;
}

void EcanInterface::applyConfig(const MeasurementInterface &mi) {
    _settings = mi;
    _host = mi.host();
    _port = mi.port();
}

unsigned EcanInterface::getBitrate() {
    return _settings.bitrate();
}

uint32_t EcanInterface::getCapabilities() {
    // The CAN bitrate is set on the gateway hardware out-of-band; this driver
    // cannot configure timing over the wire, so it advertises no config caps.
    return 0;
}

void EcanInterface::open() {
    if (_socket != NULL) {
        delete _socket;
        _socket = NULL;
    }

    _rxbuf.clear();

    _socket = new QTcpSocket();
    _socket->connectToHost(_host, _port);
    if (_socket->waitForConnected(2000)) {
        _socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
        _isOpen = true;
    } else {
        fprintf(stderr, "ECAN: connect to %s:%d failed: %s\r\n",
                _host.toStdString().c_str(), _port,
                _socket->errorString().toStdString().c_str());
        delete _socket;
        _socket = NULL;
        _isOpen = false;
    }
}

void EcanInterface::close() {
    if (_socket != NULL) {
        if (_socket->state() == QAbstractSocket::ConnectedState) {
            _socket->flush();
            _socket->disconnectFromHost();
        }
        _socket->close();
        delete _socket;
        _socket = NULL;
    }
    _isOpen = false;
}

bool EcanInterface::isOpen() {
    return _isOpen;
}

void EcanInterface::sendMessage(const CanMessage &msg) {
    // Called from the GUI thread: only enqueue here, never touch the socket
    // (the socket lives in the listener thread). The queued packet is flushed
    // inside readMessage().
    QByteArray pkt(ECAN_FRAME_LEN, 0);

    uint8_t len = msg.getLength();
    if (len > 8) len = 8;

    uint8_t info = len & 0x0F;
    if (msg.isExtended()) info |= ECAN_INFO_EXT;
    if (msg.isRTR())      info |= ECAN_INFO_RTR;
    pkt[0] = (char)info;

    uint32_t id = msg.getId();
    pkt[1] = (char)((id >> 24) & 0xFF);
    pkt[2] = (char)((id >> 16) & 0xFF);
    pkt[3] = (char)((id >> 8)  & 0xFF);
    pkt[4] = (char)( id        & 0xFF);

    for (uint8_t i=0; i<len; i++) {
        pkt[5 + i] = (char)msg.getByte(i);
    }

    QMutexLocker locker(&_txMutex);
    _txQueue.append(pkt);
}

bool EcanInterface::readMessage(QList<CanMessage> &msglist, unsigned int timeout_ms) {
    if (!_isOpen || _socket == NULL) {
        QThread::msleep(1);
        return false;
    }

    // 1) Flush queued TX (listener thread owns the socket).
    {
        QMutexLocker locker(&_txMutex);
        while (!_txQueue.isEmpty()) {
            QByteArray pkt = _txQueue.takeFirst();
            if (_socket->write(pkt) == pkt.size()) {
                _tx_count++;
            } else {
                _tx_errors++;
            }
        }
        _socket->flush();
    }

    // 2) Pull whatever is available into the reassembly buffer.
    _socket->waitForReadyRead(timeout_ms > 0 ? 1 : 0);
    if (_socket->bytesAvailable() > 0) {
        _rxbuf.append(_socket->readAll());
    }

    // 3) Extract complete 13-byte frames, resyncing on bad info bytes.
    bool got = false;
    while (_rxbuf.size() >= ECAN_FRAME_LEN) {
        uint8_t info = (uint8_t)_rxbuf.at(0);
        uint8_t hi = info & 0xF0;
        if (hi != 0x00 && hi != ECAN_INFO_RTR && hi != ECAN_INFO_EXT &&
            hi != (ECAN_INFO_EXT | ECAN_INFO_RTR)) {
            _rxbuf.remove(0, 1);
            _rx_errors++;
            continue;
        }

        uint8_t len = info & 0x0F;
        if (len > 8) len = 8;

        uint32_t id =
            ((uint32_t)(uint8_t)_rxbuf.at(1) << 24) |
            ((uint32_t)(uint8_t)_rxbuf.at(2) << 16) |
            ((uint32_t)(uint8_t)_rxbuf.at(3) << 8)  |
             (uint32_t)(uint8_t)_rxbuf.at(4);

        CanMessage msg;
        struct timeval tv;
        gettimeofday(&tv, NULL);
        msg.setTimestamp(tv);
        msg.setInterfaceId(getId());
        msg.setId(id);
        msg.setExtended(info & ECAN_INFO_EXT);
        msg.setRTR(info & ECAN_INFO_RTR);
        msg.setLength(len);
        for (uint8_t i=0; i<len; i++) {
            msg.setDataAt(i, (uint8_t)_rxbuf.at(5 + i));
        }

        msglist.append(msg);
        _rxbuf.remove(0, ECAN_FRAME_LEN);
        _rx_count++;
        got = true;
    }

    return got;
}

uint32_t EcanInterface::getState() {
    return _isOpen ? state_ok : state_bus_off;
}

int EcanInterface::getNumRxFrames()   { return _rx_count; }
int EcanInterface::getNumRxErrors()   { return _rx_errors; }
int EcanInterface::getNumRxOverruns() { return 0; }
int EcanInterface::getNumTxFrames()   { return _tx_count; }
int EcanInterface::getNumTxErrors()   { return _tx_errors; }
int EcanInterface::getNumTxDropped()  { return 0; }

int EcanInterface::getIfIndex() {
    return _idx;
}
