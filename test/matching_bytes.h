#pragma once

#include "rs485/packet_info.h"

class PacketMatchingBytes: public PacketInfo {
public:
  PacketStatus isPacket(const RS485BusBase& bus, const int startIndex, int& endIndex) const;
};