#include "matching_bytes.h"
#include "inttypes.h"

IsPacketResult ProtocolMatchingBytes::isPacket(const RS485BusBase& bus, const size_t startIndex, size_t endIndex) const {
  int16_t startingValue = bus[startIndex];
  for(size_t i = startIndex + 1; i <= endIndex; i++) {
    if(bus[i] == startingValue) {
      return {PacketStatus::YES, (i - startIndex + 1)};
    }
  }

  if(startingValue % 2 == 0) {
    return {PacketStatus::NOT_ENOUGH_BYTES, 0};
  } else {
    return {PacketStatus::NO, 0};
  }
}