#pragma once

#include "rs485bus_base.h"

class Packetizer {
public:
  explicit Packetizer(RS485BusBase& bus);

  bool hasPacket();

private:
  RS485BusBase* bus;
};