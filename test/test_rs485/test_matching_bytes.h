#pragma once

#include "../assertable_buffer.h"
#include "../matching_bytes.h"
#include "../fixtures.h"

#include "rs485bus.hpp"
#include <ArduinoFake.h>

class PacketMatchingBytesTest : public PrepBus {
public:
  PacketMatchingBytesTest(): PrepBus(),
    bus(buffer, readEnablePin, writeEnablePin) {}

  void SetUp() {
    ArduinoFake().ClearInvocationHistory();
  };

  AssertableBuffer buffer;
  RS485Bus<8> bus;
  PacketMatchingBytes packetInfo;
};

TEST_F(PacketMatchingBytesTest, single_byte_odd) {
  buffer << 0x01;
  bus.fetch();

  int endIndex = 0;
  EXPECT_EQ(packetInfo.isPacket(bus, 0, endIndex), PacketStatus::NO);
}

TEST_F(PacketMatchingBytesTest, single_byte_even) {
  buffer << 0x02;
  bus.fetch();

  int endIndex = 0;
  EXPECT_EQ(packetInfo.isPacket(bus, 0, endIndex), PacketStatus::NOT_ENOUGH_BYTES);
}

TEST_F(PacketMatchingBytesTest, various_packets) {
  buffer << 0x01 << 0x02 << 0x03 << 0x01 << 0xff << 0xfe << 0x02;
  bus.fetch();

  int endIndex = 6;
  EXPECT_EQ(packetInfo.isPacket(bus, 0, endIndex), PacketStatus::YES);
  EXPECT_EQ(endIndex, 3);

  endIndex = 6;
  EXPECT_EQ(packetInfo.isPacket(bus, 1, endIndex), PacketStatus::YES);
  EXPECT_EQ(endIndex, 6);

  EXPECT_EQ(packetInfo.isPacket(bus, 2, endIndex), PacketStatus::NO);
  EXPECT_EQ(endIndex, 6);

  EXPECT_EQ(packetInfo.isPacket(bus, 3, endIndex), PacketStatus::NO);
  EXPECT_EQ(endIndex, 6);

  EXPECT_EQ(packetInfo.isPacket(bus, 4, endIndex), PacketStatus::NO);
  EXPECT_EQ(endIndex, 6);

  EXPECT_EQ(packetInfo.isPacket(bus, 5, endIndex), PacketStatus::NOT_ENOUGH_BYTES);
  EXPECT_EQ(endIndex, 6);

  EXPECT_EQ(packetInfo.isPacket(bus, 6, endIndex), PacketStatus::NOT_ENOUGH_BYTES);
  EXPECT_EQ(endIndex, 6);
}
