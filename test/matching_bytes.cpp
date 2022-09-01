#include "matching_bytes.h"
#include "inttypes.h"

PacketStatus ProtocolMatchingBytes::isPacket(const RS485BusBase& bus, const size_t startIndex, size_t& endIndex) const {
  int16_t startingValue = bus[startIndex];
  for(size_t i = startIndex + 1; i <= endIndex; i++) {
    if(bus[i] == startingValue) {
      endIndex = i;
      return PacketStatus::YES;
    }
  }

  if(startingValue % 2 == 0) {
    return PacketStatus::NOT_ENOUGH_BYTES;
  } else {
    return PacketStatus::NO;
  }
}