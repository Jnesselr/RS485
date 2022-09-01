#pragma once

#include "../assertable_buffer.h"
#include "../matching_bytes.h"
#include "../fixtures.h"

#include <ArduinoFake.h>

#include "rs485/rs485bus.hpp"

class ProtocolMatchingBytesTest : public PrepBus {
public:
  ProtocolMatchingBytesTest(): PrepBus(),
    bus(buffer, readEnablePin, writeEnablePin) {}

  void SetUp() {
    ArduinoFake().ClearInvocationHistory();
  };

  AssertableBuffer buffer;
  RS485Bus<8> bus;
  ProtocolMatchingBytes protocol;
};

TEST_F(ProtocolMatchingBytesTest, single_byte_odd) {
  buffer << 0x01;
  bus.fetch();

  int endIndex = 0;
  EXPECT_EQ(protocol.isPacket(bus, 0, endIndex), PacketStatus::NO);
}

TEST_F(ProtocolMatchingBytesTest, single_byte_even) {
  buffer << 0x02;
  bus.fetch();

  int endIndex = 0;
  EXPECT_EQ(protocol.isPacket(bus, 0, endIndex), PacketStatus::NOT_ENOUGH_BYTES);
}

TEST_F(ProtocolMatchingBytesTest, various_packets) {
  buffer << 0x01 << 0x02 << 0x03 << 0x01 << 0xff << 0xfe << 0x02;
  bus.fetch();

  int endIndex = 6;
  EXPECT_EQ(protocol.isPacket(bus, 0, endIndex), PacketStatus::YES);
  EXPECT_EQ(endIndex, 3);

  endIndex = 6;
  EXPECT_EQ(protocol.isPacket(bus, 1, endIndex), PacketStatus::YES);
  EXPECT_EQ(endIndex, 6);

  EXPECT_EQ(protocol.isPacket(bus, 2, endIndex), PacketStatus::NO);
  EXPECT_EQ(endIndex, 6);

  EXPECT_EQ(protocol.isPacket(bus, 3, endIndex), PacketStatus::NO);
  EXPECT_EQ(endIndex, 6);

  EXPECT_EQ(protocol.isPacket(bus, 4, endIndex), PacketStatus::NO);
  EXPECT_EQ(endIndex, 6);

  EXPECT_EQ(protocol.isPacket(bus, 5, endIndex), PacketStatus::NOT_ENOUGH_BYTES);
  EXPECT_EQ(endIndex, 6);

  EXPECT_EQ(protocol.isPacket(bus, 6, endIndex), PacketStatus::NOT_ENOUGH_BYTES);
  EXPECT_EQ(endIndex, 6);
}
