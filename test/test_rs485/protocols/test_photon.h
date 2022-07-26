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
  busIO.readable<6>({0x00, 0x02, 0x1B, 0x00, 0xAB, 0x14});
  bus.fetch();

  IsPacketResult result = protocol.isPacket(bus, 0, bus.available() - 1);
  EXPECT_EQ(PacketStatus::YES, result.status);
  EXPECT_EQ(6, result.packetLength);
}

// Check every checksum BUT our valid one
TEST_F(PhotonProtocolTest, invalid_checksum) {
  uint16_t checksum = 0;
  do {
    busIO.readable<4>({0x00, 0x02, 0x1B, 0x00});
    busIO << (uint8_t) ((checksum >> 8) & 0xff);
    busIO << (uint8_t) (checksum & 0xff);
    bus.fetch();

    IsPacketResult result = protocol.isPacket(bus, 0, bus.available() - 1);
    EXPECT_EQ(PacketStatus::NO, result.status);
    EXPECT_EQ(0, result.packetLength);
    checksum++;

    if(checksum == 0xAB14) {  // Our valid checksum
      checksum++;  // Skip it
      continue;
    }
  } while(checksum != 0);
}

TEST_F(PhotonProtocolTest, returns_not_enough_bytes_if_cannot_see_length) {
  busIO << 0x00;
  bus.fetch();

  IsPacketResult result = protocol.isPacket(bus, 0, bus.available() - 1);
  EXPECT_EQ(PacketStatus::NOT_ENOUGH_BYTES, result.status);
  EXPECT_EQ(0, result.packetLength);
}

TEST_F(PhotonProtocolTest, returns_not_enough_bytes_if_does_not_have_length_plus_checksum_bytes) {
  busIO.readable<5>({0x00, 0x02, 0x1B, 0x00, 0xAB});  // Missing 0x14
  bus.fetch();

  IsPacketResult result = protocol.isPacket(bus, 0, bus.available() - 1);
  EXPECT_EQ(PacketStatus::NOT_ENOUGH_BYTES, result.status);
  EXPECT_EQ(0, result.packetLength);
}

TEST_F(PhotonProtocolTest, returns_no_if_length_will_never_allow_for_packet_to_be_read) {
  // Buffer size is 8. Address and length take up 1 byte. Checksum takes up 2.
  // Max data size is 4 bytes, meaning 5 is beyond what we can check.
  busIO.readable<2>({0x00, 0x05});
  bus.fetch();

  IsPacketResult result = protocol.isPacket(bus, 0, bus.available() - 1);
  EXPECT_EQ(PacketStatus::NO, result.status);
  EXPECT_EQ(0, result.packetLength);
}