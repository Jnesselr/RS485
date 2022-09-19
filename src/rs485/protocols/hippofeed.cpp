#include "rs485/protocols/hippofeed.h"
#include "rs485/protocols/checksums/modbus_rtu.h"
#include <gtest/gtest.h>

IsPacketResult HippoFeedProtocol::isPacket(const RS485BusBase& bus, size_t startIndex, size_t endIndex) const {
  if(startIndex == endIndex) return {PacketStatus::NOT_ENOUGH_BYTES, 0};  // Cannot see the length byte

  uint8_t lengthByte = bus[startIndex + 1];
  // Address byte, length byte, and 2 checksum bytes are our 4 here.
  if(lengthByte > (bus.bufferSize() - 4)) return {PacketStatus::NO, 0};  // We will never be able to see enough bytes to verify the checksum

  size_t checksumStartIndex = startIndex + 2 + lengthByte;
  if(checksumStartIndex + 1 > endIndex) return {PacketStatus::NOT_ENOUGH_BYTES, 0};  // Cannot see the whole checksum

  ModbusRTUChecksum checksum;

  for(size_t i = startIndex; i < checksumStartIndex; i++) {
    checksum.add(bus[i]);
  }

  uint16_t checksumvalue = checksum;
  if((uint8_t) checksumvalue != bus[checksumStartIndex]) return {PacketStatus::NO, 0};
  if((uint8_t) (checksumvalue >> 8) != bus[checksumStartIndex + 1]) return {PacketStatus::NO, 0};

  size_t packetLength = checksumStartIndex + 2 - startIndex;
  return {PacketStatus::YES, packetLength};
}