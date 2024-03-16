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

  // This buffer makes it so we don't remove any bytes at the beginning, we have a valid but filtered out packet before our
  // valid and allowed packet value. We do the same thing with putting an unmatched 0x04 before our allowed 0x05 packet.
  busIO << 0x00 << 0x02 << 0x02 << 0x03 << 0x03 << 0x04 << 0x05 << 0x05 << 0x06 << 0x07 << 0x07;
  bus.fetch();
  
  ASSERT_TRUE(packetizer.hasPacketNow());
  expectPacket(3, 4);  // 0x03 - 0x03
  ASSERT_EQ(11, bus.available());
  EXPECT_EQ(0x00, bus[0]);
  EXPECT_EQ(0x02, bus[1]);
  EXPECT_EQ(0x02, bus[2]);
  EXPECT_EQ(0x03, bus[3]);  // Packet start
  EXPECT_EQ(0x03, bus[4]);  // Packet end
  EXPECT_EQ(0x04, bus[5]);
  EXPECT_EQ(0x05, bus[6]);
  EXPECT_EQ(0x05, bus[7]);
  EXPECT_EQ(0x06, bus[8]);
  EXPECT_EQ(0x07, bus[9]);
  EXPECT_EQ(0x07, bus[10]);
  EXPECT_EQ(-1, bus[11]);

  packetizer.clearPacket();

  ASSERT_TRUE(packetizer.hasPacketNow());

  expectPacket(1, 2);
  ASSERT_EQ(6, bus.available());
  EXPECT_EQ(0x04, bus[0]);
  EXPECT_EQ(0x05, bus[1]);  // Packet start
  EXPECT_EQ(0x05, bus[2]);  // Packet end
  EXPECT_EQ(0x06, bus[3]);
  EXPECT_EQ(0x07, bus[4]);
  EXPECT_EQ(0x07, bus[5]);
  EXPECT_EQ(-1, bus[6]);

  packetizer.clearPacket();

  ASSERT_FALSE(packetizer.hasPacketNow());

  expectNoPacket();
  ASSERT_EQ(3, bus.available());
  EXPECT_EQ(0x06, bus[0]);
  EXPECT_EQ(0x07, bus[1]);
  EXPECT_EQ(0x07, bus[2]);
  EXPECT_EQ(-1, bus[3]);
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

TEST_F(PacketizerFilterTest, post_filter_eats_packets_if_they_are_filtered_and_are_at_the_start) {
  /*
  This test writes paired values so they are all packets. The "no" and "not enough bytes" semantics therefore don't apply. We write many packet values out but only allow a packet
  value that is beyond our initial buffer. When we call hasPacketNow, what we expect to see is that all our packets get filtered out and our buffer gets completely cleared out
  because every single packet that's filtered out is aligned to the start.
  */

  uint8_t lastValue = bus.bufferSize() / 2;  // Assumes even buffer.
  filter.preValues.allowAll();
  filter.postValues.allow(lastValue);

  for (size_t i = 0; i <= lastValue; i++)
  {
    busIO << i << i;  // Write two bytes at a time to always get a valid packet.
  }

  ASSERT_EQ((lastValue + 1) * 2, busIO.available());

  // At this point, we have a bus that looks like: 00 00 01 01 02 02 03 03 ... 1F 1F 20 20
  // Only 0x00 through 0x1F are able to be fetched at first.

  bus.fetch();

  ASSERT_TRUE(bus.isBufferFull());

  ASSERT_FALSE(packetizer.hasPacketNow());
  ASSERT_EQ(0, bus.available());  // Everything got eaten. They were all valid packets, filtered out, that started at the 0 index

  bus.fetch();  // Start of our allowed packet should now be index 0
  ASSERT_EQ(2, bus.available());

  ASSERT_TRUE(packetizer.hasPacketNow());
  expectPacket(0, 1);
  EXPECT_EQ(lastValue, bus[0]);
  EXPECT_EQ(lastValue, bus[1]);
  EXPECT_EQ(-1, bus[2]);
}
