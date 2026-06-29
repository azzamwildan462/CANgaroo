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

#pragma once

#include "../CanInterface.h"
#include <core/MeasurementInterface.h>
#include <QByteArray>
#include <QList>
#include <QMutex>

class EcanDriver;
class QTcpSocket;

class EcanInterface: public CanInterface {
public:
    EcanInterface(EcanDriver *driver, int index, QString host, quint16 port);
    virtual ~EcanInterface();

    virtual QString getName() const;
    void setEndpoint(QString host, quint16 port);
    virtual QString getDetailsStr() const;

    virtual bool needsHostConfig() const;

    virtual void applyConfig(const MeasurementInterface &mi);

    virtual unsigned getBitrate();
    virtual uint32_t getCapabilities();

    virtual void open();
    virtual void close();
    virtual bool isOpen();

    virtual void sendMessage(const CanMessage &msg);
    virtual bool readMessage(QList<CanMessage> &msglist, unsigned int timeout_ms);

    virtual uint32_t getState();
    virtual int getNumRxFrames();
    virtual int getNumRxErrors();
    virtual int getNumRxOverruns();
    virtual int getNumTxFrames();
    virtual int getNumTxErrors();
    virtual int getNumTxDropped();

    int getIfIndex();

private:
    int _idx;
    bool _isOpen;
    QString _name;

    MeasurementInterface _settings;

    QString _host;
    quint16 _port;

    QTcpSocket *_socket;
    QByteArray _rxbuf;

    QList<QByteArray> _txQueue;
    QMutex _txMutex;

    uint64_t _rx_count;
    uint64_t _tx_count;
    uint64_t _rx_errors;
    uint64_t _tx_errors;
};
