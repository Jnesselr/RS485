#pragma once

#include "rs485bus_base.h"
#include "packet_info.h"
#include "inttypes.h"
#include "filter.h"

enum class PacketWriteStatus {
  OK,
  FAILED_INTERRUPTED,
  FAILED_BUFFER_FULL,
  FAILED_TIMEOUT
};

class Packetizer {
public:
  explicit Packetizer(RS485BusBase& bus, const PacketInfo& packetInfo);

  bool hasPacket();
  int packetLength();
  void clearPacket();

  void setMaxReadTimeout(unsigned long maxReadTimeout);

  PacketWriteStatus writePacket(const unsigned char* buffer, int bufferSize);
  void setBusQuietTime(unsigned int busQuietTime);
  void setMaxWriteTimeout(unsigned long maxWriteTimeout);

  void setFilter(const Filter& filter);
  void removeFilter();
private:
  bool hasPacketInnerLoop();

  int fetchFromBus();
  inline void eatOneByte();
  inline void rejectByte(size_t location);

  RS485BusBase* bus;
  const PacketInfo* packetInfo;
  const Filter* filter = nullptr;
  int packetSize = 0;  // TODO size_t?

  bool shouldRecheck = true;
  int lastBusAvailable = 0;
  int startIndex = 0;
  uint64_t recheckBitmap = 0;

  unsigned long maxReadTimeout = -1;  // Read forever until you find something
  unsigned long maxWriteTimeout = -1;  // Write forever until you successfully write it (except in cases of buffer full)

  unsigned long lastByteReadTime = 0;  // Last time any bytes were known to be fetched
  unsigned int busQuietTime = 0;  // How long the bus needs to go without fetching a byte
};