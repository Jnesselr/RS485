#pragma once

#include <gtest/gtest.h>

#include "../../fixtures.h"
#include "../../assertable_bus_io.hpp"
#include "rs485/rs485bus.hpp"

#include "rs485/protocols/photon.h"

class PhotonProtocolTest : public PrepBus {
public:
  PhotonProtocolTest(): PrepBus(),
    bus(busIO, readEnablePin, writeEnablePin) {}

  void SetUp() {
    ArduinoFake().ClearInvocationHistory();
  };

  AssertableBusIO busIO;
  RS485Bus<8> bus;
  PhotonProtocol protocol;
};

TEST_F(PhotonProtocolTest, valid_checksum) {
  busIO.readable<5>({0x45, 0x00, 0x01, 0x00, 0xC0});
  bus.fetch();

  IsPacketResult result = protocol.isPacket(bus, 0, bus.available() - 1);
  EXPECT_EQ(PacketStatus::YES, result.status);
  EXPECT_EQ(5, result.packetLength);
}

// Check every checksum BUT our valid one
TEST_F(PhotonProtocolTest, invalid_checksum) {
  uint8_t checksum = 0;
  do {
    busIO.readable<4>({0x45, 0x00, 0x01, 0x00});
    busIO << (uint8_t) (checksum & 0xff);
    bus.fetch();

    IsPacketResult result = protocol.isPacket(bus, 0, bus.available() - 1);
    EXPECT_EQ(PacketStatus::NO, result.status);
    EXPECT_EQ(0, result.packetLength);
    checksum++;

    if(checksum == 0xC0) {  // Our valid checksum
      checksum++;  // Skip it
      continue;
    }
  } while(checksum != 0);
}

TEST_F(PhotonProtocolTest, returns_not_enough_bytes_for_anything_under_5_bytes) {
  for(uint8_t i = 0; i < 4; i++) {
    busIO.readable(0x0);
    bus.fetch();
    IsPacketResult result = protocol.isPacket(bus, 0, bus.available() - 1);
    EXPECT_EQ(PacketStatus::NOT_ENOUGH_BYTES, result.status);
    EXPECT_EQ(0, result.packetLength);
  }

  // Valid checksum at this point is 0.
  busIO.readable(0x0);
  bus.fetch();
  IsPacketResult result = protocol.isPacket(bus, 0, bus.available() - 1);
  EXPECT_EQ(PacketStatus::YES, result.status);
  EXPECT_EQ(5, result.packetLength);
}

TEST_F(PhotonProtocolTest, returns_not_enough_bytes_if_we_only_have_header) {
  // Payload length of 1, but only header. Next byte would be 0x05 to match checksum.
  busIO.readable<5>({0x45, 0x00, 0x01, 0x01, 0x40});
  bus.fetch();

  IsPacketResult result = protocol.isPacket(bus, 0, bus.available() - 1);
  EXPECT_EQ(PacketStatus::NOT_ENOUGH_BYTES, result.status);
  EXPECT_EQ(0, result.packetLength);
}

TEST_F(PhotonProtocolTest, checksum_validates_payload_too) {
  // Payload length of 1, but only header. Next byte would be 0x05 to match checksum.
  busIO.readable<6>({0x45, 0x00, 0x01, 0x01, 0x40, 0x05});
  bus.fetch();

  IsPacketResult result = protocol.isPacket(bus, 0, bus.available() - 1);
  EXPECT_EQ(PacketStatus::YES, result.status);
  EXPECT_EQ(6, result.packetLength);
}

TEST_F(PhotonProtocolTest, returns_no_if_length_will_never_allow_for_packet_to_be_read) {
  // Buffer size is 8. Header takes up 5 bytes, meaning a payload length of 4 is beyond what we can check.
  // The below is a valid packet, we just can't fit it all in our buffer
  busIO.readable<9>({0x00, 0x01, 0x00, 0x04, 0xB3, 0x01, 0x02, 0x03, 0x04});
  bus.fetch();

  IsPacketResult result = protocol.isPacket(bus, 0, bus.available() - 1);
  EXPECT_EQ(PacketStatus::NO, result.status);
  EXPECT_EQ(0, result.packetLength);
}