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

#include "EcanDriver.h"
#include "EcanInterface.h"
#include <core/Backend.h>
#include <driver/GenericCanSetupPage.h>

#include <QtNetwork/QTcpSocket>

// The Ebyte ECAN-E01 gateway cannot be auto-discovered, so we probe a list of
// known addresses with a short TCP connect. An interface only appears when its
// gateway actually responds -- mirroring how SocketCAN/SLCAN only list devices
// that are present. Add your gateway address(es) here.
static const struct { const char *host; quint16 port; } ECAN_CANDIDATES[] = {
    { "192.168.9.20", 8882 },
};

// How long to wait for the probe TCP connect (ms). Kept short so it does not
// stall the UI when no gateway is present.
#define ECAN_PROBE_TIMEOUT_MS 300

EcanDriver::EcanDriver(Backend &backend)
  : CanDriver(backend),
    setupPage(new GenericCanSetupPage())
{
    QObject::connect(&backend, SIGNAL(onSetupDialogCreated(SetupDialog&)), setupPage, SLOT(onSetupDialogCreated(SetupDialog&)));
}

EcanDriver::~EcanDriver() {
}

bool EcanDriver::update() {
    deleteAllInterfaces();

    int idx = 0;
    for (unsigned c=0; c<sizeof(ECAN_CANDIDATES)/sizeof(ECAN_CANDIDATES[0]); c++) {
        QString host = ECAN_CANDIDATES[c].host;
        quint16 port = ECAN_CANDIDATES[c].port;

        QTcpSocket probe;
        probe.connectToHost(host, port);
        if (probe.waitForConnected(ECAN_PROBE_TIMEOUT_MS)) {
            probe.abort();
            createOrUpdateInterface(idx++, host, port);
        }
    }

    return true;
}

QString EcanDriver::getName() {
    return "Ebyte ECAN";
}

EcanInterface *EcanDriver::createOrUpdateInterface(int index, QString host, quint16 port) {
    foreach (CanInterface *intf, getInterfaces()) {
        EcanInterface *ecif = dynamic_cast<EcanInterface*>(intf);
        if (ecif && ecif->getIfIndex() == index) {
            ecif->setEndpoint(host, port);
            return ecif;
        }
    }

    EcanInterface *ecif = new EcanInterface(this, index, host, port);
    addInterface(ecif);
    return ecif;
}
