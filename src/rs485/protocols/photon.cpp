#include "rs485/protocols/photon.h"
#include "rs485/protocols/checksums/crc8_107.h"

/**
 * Header:
    uint8_t toAddress;
    uint8_t fromAddress;
    uint8_t packetId;
    uint8_t payloadLength;
    uint8_t crc;
*/
IsPacketResult PhotonProtocol::isPacket(const RS485BusBase& bus, size_t startIndex, size_t endIndex) const {
  if(startIndex + 4 > endIndex) {
    // We can't even read the header
    return {PacketStatus::NOT_ENOUGH_BYTES, 0};
  }

  uint8_t payloadLength = bus[startIndex + 3];

  if(5 + payloadLength > bus.bufferSize()) {
    return {PacketStatus::NO, 0};
  }

  // If we don't have a payload, this is just the index of the crc
  size_t payloadEndIndex = startIndex + 4 + payloadLength;

  if(endIndex < payloadEndIndex) {
    return {PacketStatus::NOT_ENOUGH_BYTES, 0};
  }

  uint8_t seenChecksum = bus[startIndex + 4];

  CRC8_107 checksum;

  for(size_t i = 0; i < 4; i++) {
    checksum.add(bus[startIndex + i]);
  }

  for(size_t i = 0; i < payloadLength; i++) {
    checksum.add(bus[startIndex + 5 + i]);
  }

  uint8_t actualChecksum = checksum.getChecksum();

  if(actualChecksum == seenChecksum) {
    size_t packetLength = 5 + payloadLength;  // Header + Payload is our full packet
    return {PacketStatus::YES, packetLength};
  } else {
    return {PacketStatus::NO, 0};
  }
}