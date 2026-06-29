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

#include <QString>
#include <core/Backend.h>
#include <driver/CanDriver.h>

class EcanInterface;
class GenericCanSetupPage;

/* Driver for the Ebyte ECAN-E01 Ethernet-to-CAN gateway. The gateway is a TCP
   server streaming CAN frames in a fixed 13-byte binary framing. There is no
   discovery protocol, so a fixed set of selectable interface slots is exposed;
   the actual gateway IP/port is configured per-interface in the setup page. */
class EcanDriver: public CanDriver {
public:
    EcanDriver(Backend &backend);
    virtual ~EcanDriver();

    virtual QString getName();
    virtual bool update();

private:
    EcanInterface *createOrUpdateInterface(int index, QString host, quint16 port);
    GenericCanSetupPage *setupPage;
};
