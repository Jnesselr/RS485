#pragma once

#include "inttypes.h"
#include "rs485/rs485bus_base.h"
#include "rs485/protocol.h"
#include "rs485/filter.h"

enum class PacketWriteStatus {
  OK,
  FAILED_INTERRUPTED,
  FAILED_BUFFER_FULL,
  FAILED_TIMEOUT
};

class Packetizer {
public:
  explicit Packetizer(RS485BusBase& bus, const Protocol& protocol);

  bool hasPacket();
  size_t packetLength();
  void clearPacket();

  void setMaxReadTimeout(unsigned long maxReadTimeout);

  PacketWriteStatus writePacket(const uint8_t* buffer, size_t bufferSize);
  void setBusQuietTime(unsigned long busQuietTime);
  void setMaxWriteTimeout(unsigned long maxWriteTimeout);

  void setFilter(const Filter& filter);
  void removeFilter();
private:
  bool hasPacketInnerLoop();

  size_t fetchFromBus();
  inline void eatOneByte();
  inline void rejectByte(size_t location);

  RS485BusBase* bus;
  const Protocol* protocol;
  size_t packetSize = 0;

  const Filter* filter = nullptr;
  size_t filterLookAhead = 0;

  bool shouldRecheck = true;
  size_t lastBusAvailable = 0;
  size_t startIndex = 0;
  uint64_t recheckBitmap = 0;

  unsigned long maxReadTimeout = -1;
  unsigned long maxWriteTimeout = -1;

  unsigned long lastByteReadTime = 0;  // Last time any bytes were known to be fetched
  unsigned long busQuietTime = 0;  // How long the bus needs to go without fetching a byte
};