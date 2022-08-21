#include "matching_bytes.h"

PacketStatus PacketMatchingBytes::isPacket(const RS485BusBase& bus, const int startIndex, int& endIndex) const {
  int startingValue = bus[startIndex];
  for(unsigned int i = startIndex + 1; i <= endIndex; i++) {
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