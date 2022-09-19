#pragma once

#include "../assertable_bus_io.hpp"
#include "../matching_bytes.h"
#include "../fixtures.h"

#include <ArduinoFake.h>

#include "rs485/rs485bus.hpp"

class ProtocolMatchingBytesTest : public PrepBus {
public:
  ProtocolMatchingBytesTest(): PrepBus(),
    bus(busIO, readEnablePin, writeEnablePin) {}

  void SetUp() {
    ArduinoFake().ClearInvocationHistory();
  };

  AssertableBusIO busIO;
  RS485Bus<8> bus;
  ProtocolMatchingBytes protocol;
};

TEST_F(ProtocolMatchingBytesTest, single_byte_odd) {
  busIO << 0x01;
  bus.fetch();

  IsPacketResult result = protocol.isPacket(bus, 0, bus.available()-1);
  EXPECT_EQ(PacketStatus::NO, result.status);
  EXPECT_EQ(0, result.packetLength);
}

TEST_F(ProtocolMatchingBytesTest, single_byte_even) {
  busIO << 0x02;
  bus.fetch();

  IsPacketResult result = protocol.isPacket(bus, 0, bus.available()-1);
  EXPECT_EQ(PacketStatus::NOT_ENOUGH_BYTES, result.status);
  EXPECT_EQ(0, result.packetLength);
}

TEST_F(ProtocolMatchingBytesTest, various_packets) {
  busIO << 0x01 << 0x02 << 0x03 << 0x01 << 0xff << 0xfe << 0x02;
  bus.fetch();

  IsPacketResult result = protocol.isPacket(bus, 0, bus.available()-1);
  EXPECT_EQ(PacketStatus::YES, result.status);
  EXPECT_EQ(4, result.packetLength);

  result = protocol.isPacket(bus, 1, bus.available()-1);
  EXPECT_EQ(PacketStatus::YES, result.status);
  EXPECT_EQ(6, result.packetLength);

  result = protocol.isPacket(bus, 2, bus.available()-1);
  EXPECT_EQ(PacketStatus::NO, result.status);
  EXPECT_EQ(0, result.packetLength);

  result = protocol.isPacket(bus, 3, bus.available()-1);
  EXPECT_EQ(PacketStatus::NO, result.status);
  EXPECT_EQ(0, result.packetLength);

  result = protocol.isPacket(bus, 4, bus.available()-1);
  EXPECT_EQ(PacketStatus::NO, result.status);
  EXPECT_EQ(0, result.packetLength);

  result = protocol.isPacket(bus, 5, bus.available()-1);
  EXPECT_EQ(PacketStatus::NOT_ENOUGH_BYTES, result.status);
  EXPECT_EQ(0, result.packetLength);

  result = protocol.isPacket(bus, 6, bus.available()-1);
  EXPECT_EQ(PacketStatus::NOT_ENOUGH_BYTES, result.status);
  EXPECT_EQ(0, result.packetLength);
}
