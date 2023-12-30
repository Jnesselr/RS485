#pragma once


#include "../assertable_bus_io.hpp"
#include "../matching_bytes.h"
#include "../fixtures.h"

#include <gtest/gtest.h>
#include "fakeit/fakeit.hpp"

#include "rs485/rs485bus.hpp"
#include "rs485/packetizer.h"
#include "rs485/filters/filter_by_value.h"

using namespace fakeit;

class PacketizerFilterTest : public PrepBus {
protected:
  PacketizerFilterTest() : PrepBus(),
    bus(busIO, readEnablePin, writeEnablePin),
    packetizer(bus, protocol),
    filter(0)
    {
      packetizer.setFilter(filter);
    }

  void SetUp() {
    PrepBus::SetUp();
    When(Method(ArduinoFake(), micros)).AlwaysDo([&]()->unsigned long{
      currentMicros++;
      return currentMicros;
    });
    packetizer.setMaxReadTimeout(20);
  }

  void expectNoPacket() {
    Packet packet = packetizer.getPacket();
    EXPECT_EQ(0, packet.startIndex);
    EXPECT_EQ(0, packet.endIndex);
  }

  void expectPacket(size_t startIndex, size_t endIndex) {
    Packet packet = packetizer.getPacket();
    EXPECT_EQ(startIndex, packet.startIndex);
    EXPECT_EQ(endIndex, packet.endIndex);
  }

  volatile unsigned long currentMicros = 0;
  AssertableBusIO busIO;
  RS485Bus<64> bus;
  ProtocolMatchingBytes protocol;
  Packetizer packetizer;
  FilterByValue filter;
};

TEST_F(PacketizerFilterTest, pre_filter_is_called_before_is_packet) {
  filter.preValues.allow(5);
  filter.postValues.allowAll();

  // If preFilter isn't called, this would result in a packet surrounding the first 0x05
  busIO << 0x01 << 0x05 << 0x01 << 0x05;
  bus.fetch();

  ASSERT_TRUE(packetizer.hasPacketNow());
  expectPacket(0, 2);
  ASSERT_EQ(3, bus.available());
  EXPECT_EQ(0x05, bus[0]);
  EXPECT_EQ(0x01, bus[1]);
  EXPECT_EQ(0x05, bus[2]);
  EXPECT_EQ(-1, bus[3]);
}

TEST_F(PacketizerFilterTest, pre_filter_is_ignored_if_filter_is_disabled) {
  filter.preValues.allow(5);
  filter.postValues.allowAll();
  filter.setEnabled(false);

  // The filter is disabled, so the <0x01 .. 0x01> packet is found
  busIO << 0x01 << 0x05 << 0x01 << 0x05;
  bus.fetch();

  ASSERT_TRUE(packetizer.hasPacketNow());
  expectPacket(0, 2);
  ASSERT_EQ(4, bus.available());
  EXPECT_EQ(0x01, bus[0]);
  EXPECT_EQ(0x05, bus[1]);
  EXPECT_EQ(0x01, bus[2]);
  EXPECT_EQ(0x05, bus[3]);
  EXPECT_EQ(-1, bus[4]);
}

TEST_F(PacketizerFilterTest, pre_filter_rejection_is_permanent) {
  filter.preValues.allowAll();
  filter.postValues.allowAll();

  // 0x04 is kept by filter and protocol
  busIO << 0x04;
  bus.fetch();

  ASSERT_FALSE(packetizer.hasPacketNow());
  expectNoPacket();
  // bus: 04
  ASSERT_EQ(1, bus.available());
  EXPECT_EQ(0x04, bus[0]);
  EXPECT_EQ(-1, bus[1]);

  // 0x02 is rejected by filter, not that it has a matching byte to form a packet anyway.
  filter.preValues.reject(0x02);
  busIO << 0x02;
  bus.fetch();

  // bus: 04 02

  ASSERT_FALSE(packetizer.hasPacketNow());
  expectNoPacket();
  ASSERT_EQ(2, bus.available());
  EXPECT_EQ(0x04, bus[0]);
  EXPECT_EQ(0x02, bus[1]);
  EXPECT_EQ(-1, bus[2]);

  // 0x02 would make a matching packet, if 0x02 before hadn't already been filtered out.
  filter.preValues.allow(0x02);
  busIO << 0x02;
  bus.fetch();

  // bus: 04 02 02

  ASSERT_FALSE(packetizer.hasPacketNow());
  expectNoPacket();
  ASSERT_EQ(3, bus.available());
  EXPECT_EQ(0x04, bus[0]);
  EXPECT_EQ(0x02, bus[1]);
  EXPECT_EQ(0x02, bus[2]);
  EXPECT_EQ(-1, bus[3]);

  // The new 0x02 DOES make a matching packet now though
  busIO << 0x02;
  bus.fetch();

  // bus: 04 02 02 02

  ASSERT_TRUE(packetizer.hasPacketNow());
  expectPacket(2, 3);

  ASSERT_EQ(4, bus.available());
  EXPECT_EQ(0x04, bus[0]);
  EXPECT_EQ(0x02, bus[1]);
  EXPECT_EQ(0x02, bus[2]);
  EXPECT_EQ(0x02, bus[3]);
  EXPECT_EQ(-1, bus[4]);
}

TEST_F(PacketizerFilterTest, post_filter_is_called_after_is_packet) {
  filter.preValues.allowAll();
  filter.postValues.allow(0x03);
  filter.postValues.allow(0x05);

  busIO << 0x01 << 0x01 << 0x02 << 0x02 << 0x03 << 0x03 << 0x04 << 0x04 << 0x05 << 0x05 << 0x06 << 0x06 << 0x07 << 0x07;
  bus.fetch();
  
  ASSERT_TRUE(packetizer.hasPacketNow());
  expectPacket(0, 1);
  ASSERT_GT(bus.available(), 2);  // We don't care what anything else past this is at the moment, we just care about our packet
  EXPECT_EQ(0x03, bus[0]);
  EXPECT_EQ(0x03, bus[1]);

  packetizer.clearPacket();

  ASSERT_TRUE(packetizer.hasPacketNow());
  expectPacket(0, 1);
  ASSERT_EQ(6, bus.available());
  EXPECT_EQ(0x05, bus[0]);
  EXPECT_EQ(0x05, bus[1]);
  EXPECT_EQ(0x06, bus[2]);
  EXPECT_EQ(0x06, bus[3]);
  EXPECT_EQ(0x07, bus[4]);
  EXPECT_EQ(0x07, bus[5]);
  EXPECT_EQ(-1, bus[6]);

  packetizer.clearPacket();

  ASSERT_FALSE(packetizer.hasPacketNow());  // This will have wiped away everything else
  expectNoPacket();
  EXPECT_EQ(0, bus.available());
}

TEST_F(PacketizerFilterTest, post_filter_is_ignored_if_filter_is_disabled) {
  filter.preValues.allowAll();
  filter.postValues.allow(0x02);
  filter.setEnabled(false);

  busIO << 0x01 << 0x01 << 0x02 << 0x02;
  bus.fetch();
  
  ASSERT_TRUE(packetizer.hasPacketNow());
  expectPacket(0, 1);
  ASSERT_EQ(bus.available(), 4);
  EXPECT_EQ(0x01, bus[0]);
  EXPECT_EQ(0x01, bus[1]);
  EXPECT_EQ(0x02, bus[2]);
  EXPECT_EQ(0x02, bus[3]);
  EXPECT_EQ(-1, bus[4]);

  packetizer.clearPacket();

  ASSERT_TRUE(packetizer.hasPacketNow());
  expectPacket(0, 1);
  ASSERT_EQ(2, bus.available());
  EXPECT_EQ(0x02, bus[0]);
  EXPECT_EQ(0x02, bus[1]);
  EXPECT_EQ(-1, bus[2]);

  packetizer.clearPacket();

  ASSERT_FALSE(packetizer.hasPacketNow());  // This will have wiped away everything else
  expectNoPacket();
  EXPECT_EQ(0, bus.available());
}