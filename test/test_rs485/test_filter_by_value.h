#pragma once

#include <gtest/gtest.h>

#include "../assertable_buffer.h"
#include "../fixtures.h"

#include "filters/filter_by_value.h"
#include "rs485bus.hpp"

#define FOR_BYTES(name) for(unsigned char name = 0; name < 256; name++)


class FilterByValueTest : public PrepBus {
protected:
  FilterByValueTest(): PrepBus(),
    filterAhead5(5),
    bus(buffer, readEnablePin, writeEnablePin) {}

  // using bytesFunction = void (*)(unsigned char);
  template<typename T>
  void forBytes(const T &func) {
    uint8_t i = 0;
    do {
      buffer << i;
      bus.fetch();

      func(i);

      while(bus.read() >= 0) {}  // Clear the bus
      i++;
    } while(i != 0);
  }

  FilterByValue filter;
  FilterByValue filterAhead5;
  AssertableBuffer buffer;
  RS485Bus<8> bus;
};

TEST_F(FilterByValueTest, by_default_look_ahead_is_0) {
  EXPECT_EQ(filter.lookAheadBytes(), 0);
  EXPECT_EQ(filterAhead5.lookAheadBytes(), 5);
}

TEST_F(FilterByValueTest, by_default_pre_value_allows_nothing) {
  forBytes([this](uint8_t i) {
    uint8_t byteOffset = i >> 3;
    ASSERT_FALSE(filter.preFilter(bus, 0)) << "Filter should reject " << i;
  });
}

TEST_F(FilterByValueTest, pre_filter_values_can_be_changed) {
  forBytes([this](uint8_t i) {
    filter.preValues.allow(i);
    EXPECT_TRUE(filter.preFilter(bus, 0)) << "Filter should allow " << i;

    filter.preValues.reject(i);
    EXPECT_FALSE(filter.preFilter(bus, 0)) << "Filter should reject " << i;
  });
}

TEST_F(FilterByValueTest, pre_filter_allow_everything) {
  filter.preValues.allowAll();
  forBytes([this](uint8_t i) {
    EXPECT_TRUE(filter.preFilter(bus, 0)) << "Filter should allow " << i;
  });
}

TEST_F(FilterByValueTest, pre_filter_allow_nothing) {
  filter.preValues.allowAll();
  filter.preValues.rejectAll();
  forBytes([this](uint8_t i) {
    EXPECT_FALSE(filter.preFilter(bus, 0)) << "Filter should reject " << i;
  });
}

TEST_F(FilterByValueTest, pre_filter_respects_look_ahead) {

  // We already have i on the bus, so we'll write 4 more i bytes and then check for i + 1.
  // The bus will look like: i i i i i (i+1)
  forBytes([this](uint8_t i) {
    uint8_t plusOne = i + 1;
    filterAhead5.preValues.rejectAll();
    filterAhead5.preValues.allow(plusOne);
    buffer << i << i << i << i << plusOne;
    bus.fetch();

    EXPECT_TRUE(filterAhead5.preFilter(bus, 0)) << "Filter should allow " << plusOne;
  });
}

TEST_F(FilterByValueTest, pre_filter_respects_start_index) {

  // We already have i on the bus, so we'll write 4 more i bytes and then check for i + 1.
  // The bus will look like: i i i i i (i+1)
  forBytes([this](uint8_t i) {
    uint8_t plusOne = i + 1;
    filterAhead5.preValues.rejectAll();
    filterAhead5.preValues.allow(plusOne);
    buffer << i << i << i << i << i << plusOne;
    bus.fetch();

    EXPECT_FALSE(filterAhead5.preFilter(bus, 0)) << "Filter should reject " << i;
    EXPECT_TRUE(filterAhead5.preFilter(bus, 1)) << "Filter should allow " << plusOne;
  });
}

TEST_F(FilterByValueTest, by_default_post_value_allows_nothing) {
  forBytes([this](uint8_t i) {
    uint8_t byteOffset = i >> 3;
    ASSERT_FALSE(filter.postFilter(bus, 0, 0)) << "Filter should reject " << i;
  });
}

TEST_F(FilterByValueTest, post_filter_values_can_be_changed) {
  forBytes([this](uint8_t i) {
    filter.postValues.allow(i);
    EXPECT_TRUE(filter.postFilter(bus, 0, 0)) << "Filter should allow " << i;

    filter.postValues.reject(i);
    EXPECT_FALSE(filter.postFilter(bus, 0, 0)) << "Filter should reject " << i;
  });
}

TEST_F(FilterByValueTest, post_filter_allow_everything) {
  filter.postValues.allowAll();
  forBytes([this](uint8_t i) {
    EXPECT_TRUE(filter.postFilter(bus, 0, 0)) << "Filter should allow " << i;
  });
}

TEST_F(FilterByValueTest, post_filter_allow_nothing) {
  filter.postValues.allowAll();
  filter.postValues.rejectAll();
  forBytes([this](uint8_t i) {
    EXPECT_FALSE(filter.postFilter(bus, 0, 0)) << "Filter should reject " << i;
  });
}

TEST_F(FilterByValueTest, post_filter_respects_look_ahead) {

  // We already have i on the bus, so we'll write 4 more i bytes and then check for i + 1.
  // The bus will look like: i i i i i (i+1)
  forBytes([this](uint8_t i) {
    uint8_t plusOne = i + 1;
    filterAhead5.postValues.rejectAll();
    filterAhead5.postValues.allow(plusOne);
    buffer << i << i << i << i << plusOne;
    bus.fetch();

    EXPECT_TRUE(filterAhead5.postFilter(bus, 0, 5)) << "Filter should allow " << plusOne;
  });
}

TEST_F(FilterByValueTest, post_filter_respects_start_index) {

  // We already have i on the bus, so we'll write 4 more i bytes and then check for i + 1.
  // The bus will look like: i i i i i (i+1)
  forBytes([this](uint8_t i) {
    uint8_t plusOne = i + 1;
    filterAhead5.postValues.rejectAll();
    filterAhead5.postValues.allow(plusOne);
    buffer << i << i << i << i << i << plusOne;
    bus.fetch();

    EXPECT_FALSE(filterAhead5.postFilter(bus, 0, 5)) << "Filter should reject " << i;
    EXPECT_TRUE(filterAhead5.postFilter(bus, 1, 6)) << "Filter should allow " << plusOne;
  });
}

TEST_F(FilterByValueTest, post_filter_will_not_look_past_shorter_end_index) {
  filterAhead5.postValues.allowAll();
  uint8_t i = 0;
  buffer << i << i << i << i << i << i + 1;
  bus.fetch();

  EXPECT_FALSE(filterAhead5.postFilter(bus, 0, 0)) << "Filter should not search past endIndex";
  EXPECT_FALSE(filterAhead5.postFilter(bus, 0, 1)) << "Filter should not search past endIndex";
  EXPECT_FALSE(filterAhead5.postFilter(bus, 0, 2)) << "Filter should not search past endIndex";
  EXPECT_FALSE(filterAhead5.postFilter(bus, 0, 3)) << "Filter should not search past endIndex";
  EXPECT_FALSE(filterAhead5.postFilter(bus, 0, 4)) << "Filter should not search past endIndex";
  EXPECT_TRUE(filterAhead5.postFilter(bus, 0, 5)) << "Filter should search with enough bytes";
}

TEST_F(FilterByValueTest, post_filter_will_not_look_past_shorter_end_index_with_nonzero_start_index) {
  filterAhead5.postValues.allowAll();
  uint8_t i = 0;
  buffer << i << i << i << i << i << i << i + 1;
  bus.fetch();

  EXPECT_FALSE(filterAhead5.postFilter(bus, 1, 1)) << "Filter should not search past endIndex";
  EXPECT_FALSE(filterAhead5.postFilter(bus, 1, 2)) << "Filter should not search past endIndex";
  EXPECT_FALSE(filterAhead5.postFilter(bus, 1, 3)) << "Filter should not search past endIndex";
  EXPECT_FALSE(filterAhead5.postFilter(bus, 1, 4)) << "Filter should not search past endIndex";
  EXPECT_FALSE(filterAhead5.postFilter(bus, 1, 5)) << "Filter should not search past endIndex";
  EXPECT_TRUE(filterAhead5.postFilter(bus, 1, 6)) << "Filter should search with enough bytes";
}