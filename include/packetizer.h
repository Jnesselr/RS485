#pragma once

#include "rs485bus_base.h"
#include "packet_info.h"

class Packetizer {
public:
  explicit Packetizer(RS485BusBase& bus, const PacketInfo& packetInfo);

  bool hasPacket();
  int packetLength();
  void clearPacket();

  void setMaxReadTimeout(unsigned long maxReadTimeout);
private:
  inline void eatOneByte();

  RS485BusBase* bus;
  const PacketInfo* packetInfo;
  int packetSize = 0;

  int lastBusAvailable = 0;
  int startIndex = 0;
  unsigned long recheckBitmap = 0;

  unsigned long maxReadTimeout = -1;  // Just go forever until you find something
};