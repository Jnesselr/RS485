#include "rs485/protocols/hippofeed.h"
#include "rs485/protocols/checksums/modbus_rtu.h"
#include <gtest/gtest.h>

PacketStatus HippoFeedProtocol::isPacket(const RS485BusBase& bus, size_t startIndex, size_t& endIndex) const {
  ModbusRTUChecksum checksum;

  if(startIndex == endIndex) return PacketStatus::NOT_ENOUGH_BYTES;  // Cannot see the length byte

  uint8_t lengthByte = bus[startIndex + 1];
  if(lengthByte > (bus.bufferSize() - 4)) return PacketStatus::NO;  // Address byte, length byte, and 2 checksum bytes are our 4 here.

  size_t checksumStartIndex = startIndex + 2 + lengthByte;
  if(checksumStartIndex + 1 > endIndex) return PacketStatus::NOT_ENOUGH_BYTES;  // Cannot see the whole checksum

  for(size_t i = startIndex; i < checksumStartIndex; i++) {
    checksum.add(bus[i]);
  }

  uint16_t checksumvalue = checksum;
  if((uint8_t) checksumvalue != bus[checksumStartIndex]) return PacketStatus::NO;
  if((uint8_t) (checksumvalue >> 8) != bus[checksumStartIndex + 1]) return PacketStatus::NO;

  return PacketStatus::YES;
}