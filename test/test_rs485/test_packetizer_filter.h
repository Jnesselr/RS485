#pragma once


#include "../assertable_buffer.h"
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
    bus(buffer, readEnablePin, writeEnablePin),
    packetizer(bus, protocol),
    filter(0),
    filterAhead5(5)
    {
      packetizer.setFilter(filter);
    }

  void SetUp() {
    PrepBus::SetUp();
    When(Method(ArduinoFake(), millis)).AlwaysReturn(0);
  }

  AssertableBuffer buffer;
  RS485Bus<8> bus;
  ProtocolMatchingBytes protocol;
  Packetizer packetizer;
  FilterByValue filter;
  FilterByValue filterAhead5;
};

TEST_F(PacketizerFilterTest, pre_filter_is_called_before_is_packet) {
  filter.preValues.allow(5);
  filter.postValues.allowAll();

  // If preFilter isn't called, this would result in a packet surrounding the first 0x05
  buffer << 0x01 << 0x05 << 0x01 << 0x05;

  ASSERT_TRUE(packetizer.hasPacket());
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
  buffer << 0x01 << 0x05 << 0x01 << 0x05;

  ASSERT_TRUE(packetizer.hasPacket());
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
  buffer << 0x04;
  ASSERT_FALSE(packetizer.hasPacket());
  // bus: 04
  ASSERT_EQ(1, bus.available());
  EXPECT_EQ(0x04, bus[0]);
  EXPECT_EQ(-1, bus[1]);

  // 0x02 is rejected by filter, not that it has a matching byte to form a packet anyway.
  filter.preValues.reject(0x02);
  buffer << 0x02;
  ASSERT_FALSE(packetizer.hasPacket());
  // bus: 04 02
  ASSERT_EQ(2, bus.available());
  EXPECT_EQ(0x04, bus[0]);
  EXPECT_EQ(0x02, bus[1]);
  EXPECT_EQ(-1, bus[2]);

  // 0x02 would make a matching packet, if 0x02 before hadn't already been filtered out.
  filter.preValues.allow(0x02);
  buffer << 0x02;
  ASSERT_FALSE(packetizer.hasPacket());
  // bus: 04 02 02
  ASSERT_EQ(3, bus.available());
  EXPECT_EQ(0x04, bus[0]);
  EXPECT_EQ(0x02, bus[1]);
  EXPECT_EQ(0x02, bus[2]);
  EXPECT_EQ(-1, bus[3]);

  // The new 0x02 DOES make a matching packet now though
  buffer << 0x02;
  ASSERT_TRUE(packetizer.hasPacket());
  // bus: 02 02

  ASSERT_EQ(2, bus.available());
  EXPECT_EQ(0x02, bus[0]);
  EXPECT_EQ(0x02, bus[1]);
  EXPECT_EQ(-1, bus[2]);
}

TEST_F(PacketizerFilterTest, pre_filter_disabled_does_not_affect_previous_rejections) {
  filter.preValues.allowAll();
  filter.preValues.reject(0x02);
  filter.postValues.allowAll();

  // 0x04 is allowed, 0x02 is not.
  buffer << 0x04 << 0x02;
  ASSERT_FALSE(packetizer.hasPacket());
  // bus: 04 02
  ASSERT_EQ(2, bus.available());
  EXPECT_EQ(0x04, bus[0]);
  EXPECT_EQ(0x02, bus[1]);
  EXPECT_EQ(-1, bus[2]);

  // 0x02 would make a matching packet if the filter hadn't already rejected the first 0x02
  filter.setEnabled(false);
  buffer << 0x02;
  ASSERT_FALSE(packetizer.hasPacket());
  // bus: 04 02 02
  ASSERT_EQ(bus.available(), 3);
  EXPECT_EQ(0x04, bus[0]);
  EXPECT_EQ(0x02, bus[1]);
  EXPECT_EQ(0x02, bus[2]);
  EXPECT_EQ(-1, bus[3]);

  // The new 0x02 DOES make a matching packet now though
  buffer << 0x02;
  ASSERT_TRUE(packetizer.hasPacket());
  // bus: 02 02

  ASSERT_EQ(2, bus.available());
  EXPECT_EQ(0x02, bus[0]);
  EXPECT_EQ(0x02, bus[1]);
  EXPECT_EQ(-1, bus[2]);
}

TEST_F(PacketizerFilterTest, post_filter_is_called_after_is_packet) {
  filter.preValues.allowAll();
  filter.postValues.allow(0x03);
  filter.postValues.allow(0x05);

  buffer << 0x01 << 0x01 << 0x02 << 0x02 << 0x03 << 0x03 << 0x04 << 0x04 << 0x05 << 0x05 << 0x06 << 0x06 << 0x07 << 0x07;
  
  ASSERT_TRUE(packetizer.hasPacket());
  EXPECT_EQ(2, packetizer.packetLength());
  ASSERT_GT(bus.available(), 2);  // We don't care what anything else past this is at the moment, we just care about our packet
  EXPECT_EQ(0x03, bus[0]);
  EXPECT_EQ(0x03, bus[1]);

  packetizer.clearPacket();

  ASSERT_TRUE(packetizer.hasPacket());
  EXPECT_EQ(2, packetizer.packetLength());
  ASSERT_EQ(6, bus.available());
  EXPECT_EQ(0x05, bus[0]);
  EXPECT_EQ(0x05, bus[1]);
  EXPECT_EQ(0x06, bus[2]);
  EXPECT_EQ(0x06, bus[3]);
  EXPECT_EQ(0x07, bus[4]);
  EXPECT_EQ(0x07, bus[5]);
  EXPECT_EQ(-1, bus[6]);

  packetizer.clearPacket();

  ASSERT_FALSE(packetizer.hasPacket());  // This will have wiped away everything else
  EXPECT_EQ(0, packetizer.packetLength());
  EXPECT_EQ(0, bus.available());
}

TEST_F(PacketizerFilterTest, post_filter_is_ignored_if_filter_is_disabled) {
  filter.preValues.allowAll();
  filter.postValues.allow(0x02);
  filter.setEnabled(false);

  buffer << 0x01 << 0x01 << 0x02 << 0x02;
  
  ASSERT_TRUE(packetizer.hasPacket());
  EXPECT_EQ(2, packetizer.packetLength());
  ASSERT_EQ(bus.available(), 4);
  EXPECT_EQ(0x01, bus[0]);
  EXPECT_EQ(0x01, bus[1]);
  EXPECT_EQ(0x02, bus[2]);
  EXPECT_EQ(0x02, bus[3]);
  EXPECT_EQ(-1, bus[4]);

  packetizer.clearPacket();

  ASSERT_TRUE(packetizer.hasPacket());
  EXPECT_EQ(2, packetizer.packetLength());
  ASSERT_EQ(2, bus.available());
  EXPECT_EQ(0x02, bus[0]);
  EXPECT_EQ(0x02, bus[1]);
  EXPECT_EQ(-1, bus[2]);

  packetizer.clearPacket();

  ASSERT_FALSE(packetizer.hasPacket());  // This will have wiped away everything else
  EXPECT_EQ(0, packetizer.packetLength());
  EXPECT_EQ(0, bus.available());
}

TEST_F(PacketizerFilterTest, look_ahead_bytes_are_respected_when_deciding_to_call_filter) {
  packetizer.setFilter(filterAhead5);
  filterAhead5.preValues.allowAll();
  filterAhead5.postValues.allowAll();

  // Even though this has a packet, we won't verify it because we won't call our filter.
  buffer << 0x01 << 0x02 << 0x03 << 0x04 << 0x01;

  ASSERT_FALSE(packetizer.hasPacket());

  buffer << 0x06 << 0x06;

  ASSERT_TRUE(packetizer.hasPacket());
  EXPECT_EQ(5, packetizer.packetLength());
  EXPECT_EQ(7, bus.available());
  EXPECT_EQ(0x01, bus[0]);
  EXPECT_EQ(0x02, bus[1]);
  EXPECT_EQ(0x03, bus[2]);
  EXPECT_EQ(0x04, bus[3]);
  EXPECT_EQ(0x01, bus[4]);
  EXPECT_EQ(0x06, bus[5]);
  EXPECT_EQ(0x06, bus[6]);
  EXPECT_EQ(-1, bus[7]);

  packetizer.clearPacket();
  // Has packet MUST be called to verify setFilter resets the last bus available variable
  ASSERT_FALSE(packetizer.hasPacket());

  // If the filter is left in place, we want it to reject our packet
  filterAhead5.preValues.rejectAll();
  filterAhead5.preValues.rejectAll();

  // Does changing the filter reset look ahead bytes?
  packetizer.setFilter(filter);
  filter.preValues.allowAll();
  filter.postValues.allowAll();

  ASSERT_TRUE(packetizer.hasPacket());
  EXPECT_EQ(2, packetizer.packetLength());
  EXPECT_EQ(2, bus.available());
  EXPECT_EQ(0x06, bus[0]);
  EXPECT_EQ(0x06, bus[1]);
  EXPECT_EQ(-1, bus[2]);
}

TEST_F(PacketizerFilterTest, look_ahead_bytes_are_removed_when_filter_is_removed) {
  packetizer.setFilter(filterAhead5);
  filterAhead5.preValues.allowAll();
  filterAhead5.postValues.allowAll();

  // Even though this has a packet, we won't verify it because we won't call our filter.
  buffer << 0x01 << 0x02 << 0x03 << 0x04 << 0x01;

  ASSERT_FALSE(packetizer.hasPacket());

  buffer << 0x06 << 0x06;

  ASSERT_TRUE(packetizer.hasPacket());
  EXPECT_EQ(5, packetizer.packetLength());
  EXPECT_EQ(7, bus.available());
  EXPECT_EQ(0x01, bus[0]);
  EXPECT_EQ(0x02, bus[1]);
  EXPECT_EQ(0x03, bus[2]);
  EXPECT_EQ(0x04, bus[3]);
  EXPECT_EQ(0x01, bus[4]);
  EXPECT_EQ(0x06, bus[5]);
  EXPECT_EQ(0x06, bus[6]);
  EXPECT_EQ(-1, bus[7]);

  packetizer.clearPacket();
  // Has packet MUST be called to verify removeFilter resets the last bus available variable
  ASSERT_FALSE(packetizer.hasPacket());

  // If the filter is left in place, we want it to reject our packet
  filterAhead5.preValues.rejectAll();
  filterAhead5.preValues.rejectAll();

  // Does removing the filter reset look ahead bytes?
  packetizer.removeFilter();

  ASSERT_TRUE(packetizer.hasPacket());
  EXPECT_EQ(2, packetizer.packetLength());
  EXPECT_EQ(2, bus.available());
  EXPECT_EQ(0x06, bus[0]);
  EXPECT_EQ(0x06, bus[1]);
  EXPECT_EQ(-1, bus[2]);
}