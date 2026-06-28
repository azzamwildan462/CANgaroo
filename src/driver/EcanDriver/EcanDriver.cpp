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

// Number of selectable ECAN gateway slots. Each slot carries its own IP/port
// via its MeasurementInterface, so bump this to talk to more gateways at once.
#define ECAN_NUM_INTERFACES 2

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

    for (int i=0; i<ECAN_NUM_INTERFACES; i++) {
        createOrUpdateInterface(i, QString("ECAN Gateway %1").arg(i+1));
    }

    return true;
}

QString EcanDriver::getName() {
    return "Ebyte ECAN";
}

EcanInterface *EcanDriver::createOrUpdateInterface(int index, QString name) {
    foreach (CanInterface *intf, getInterfaces()) {
        EcanInterface *ecif = dynamic_cast<EcanInterface*>(intf);
        if (ecif && ecif->getIfIndex() == index) {
            ecif->setName(name);
            return ecif;
        }
    }

    EcanInterface *ecif = new EcanInterface(this, index, name);
    addInterface(ecif);
    return ecif;
}
