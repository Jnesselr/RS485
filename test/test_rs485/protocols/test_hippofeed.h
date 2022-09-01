#pragma once

#include <gtest/gtest.h>

#include "../../fixtures.h"
#include "../../assertable_buffer.hpp"
#include "rs485/rs485bus.hpp"

#include "rs485/protocols/hippofeed.h"

class HippoFeedProtocolTest : public PrepBus {
public:
  HippoFeedProtocolTest(): PrepBus(),
    bus(buffer, readEnablePin, writeEnablePin) {}

  void SetUp() {
    ArduinoFake().ClearInvocationHistory();
  };

  AssertableBuffer buffer;
  RS485Bus<8> bus;
  HippoFeedProtocol protocol;
};

TEST_F(HippoFeedProtocolTest, valid_checksum) {
  buffer.readable<6>({0x00, 0x02, 0x1B, 0x00, 0xAB, 0x14});
  bus.fetch();

  size_t endIndex = bus.available() - 1;
  ASSERT_EQ(PacketStatus::YES, protocol.isPacket(bus, 0, endIndex));
}

// Check every checksum BUT our valid one
TEST_F(HippoFeedProtocolTest, invalid_checksum) {
  uint16_t checksum = 0;
  do {
    buffer.readable<4>({0x00, 0x02, 0x1B, 0x00});
    buffer << (uint8_t) ((checksum >> 8) & 0xff);
    buffer << (uint8_t) (checksum & 0xff);
    bus.fetch();

    size_t endIndex = bus.available() - 1;
    ASSERT_EQ(PacketStatus::NO, protocol.isPacket(bus, 0, endIndex));
    checksum++;

    if(checksum == 0xAB14) {  // Our valid checksum
      checksum++;  // Skip it
      continue;
    }
  } while(checksum != 0);
}

TEST_F(HippoFeedProtocolTest, returns_not_enough_bytes_if_cannot_see_length) {
  buffer << 0x00;
  bus.fetch();

  size_t endIndex = bus.available() - 1;
  ASSERT_EQ(PacketStatus::NOT_ENOUGH_BYTES, protocol.isPacket(bus, 0, endIndex));
}

TEST_F(HippoFeedProtocolTest, returns_not_enough_bytes_if_does_not_have_length_plus_checksum_bytes) {
  buffer.readable<5>({0x00, 0x02, 0x1B, 0x00, 0xAB});  // Missing 0x14
  bus.fetch();

  size_t endIndex = bus.available() - 1;
  ASSERT_EQ(PacketStatus::NOT_ENOUGH_BYTES, protocol.isPacket(bus, 0, endIndex));
}

TEST_F(HippoFeedProtocolTest, returns_no_if_length_will_never_allow_for_packet_to_be_read) {
  // Buffer size is 8. Address and length take up 1 byte. Checksum takes up 2.
  // Max data size is 4 bytes, meaning 5 is beyond what we can check.
  buffer.readable<2>({0x00, 0x05});
  bus.fetch();

  size_t endIndex = bus.available() - 1;
  ASSERT_EQ(PacketStatus::NO, protocol.isPacket(bus, 0, endIndex));
}