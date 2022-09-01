#pragma once

#include "../../assertable_buffer.hpp"
#include "../../fixtures.h"

#include <gtest/gtest.h>

#include "rs485/rs485bus.hpp"
#include "rs485/filters/filter_by_value.h"
#include "rs485/filters/combo_filter.h"


class ComboFilterTest : public PrepBus {
protected:
  ComboFilterTest(): PrepBus(),
    left(0),
    right(1),
    combo(left, right),
    bus(buffer, readEnablePin, writeEnablePin) {}

  void SetUp() {
    /**
     * startIndex and whether a given filter is valid at that index, denoted by a + for valid or - for not.
     * 
     * start left right
     *     0    +     +
     *     1    -     +
     *     2    -     -
     *     3    +     -
     */
    buffer << 0x01 << 0x02 << 0x02 << 0x01 << 0x03;
    bus.fetch();

    left.preValues.allow(0x01);
    right.preValues.allow(0x02);

    left.postValues.allow(0x01);
    right.postValues.allow(0x02);

    // Verify the above table, since most tests rely on it.
    ASSERT_TRUE(left.preFilter(bus, 0));
    ASSERT_TRUE(right.preFilter(bus, 0));
    ASSERT_FALSE(left.preFilter(bus, 1));
    ASSERT_TRUE(right.preFilter(bus, 1));
    ASSERT_FALSE(left.preFilter(bus, 2));
    ASSERT_FALSE(right.preFilter(bus, 2));
    ASSERT_TRUE(left.preFilter(bus, 3));
    ASSERT_FALSE(right.preFilter(bus, 3));

    ASSERT_TRUE(left.postFilter(bus, 0, 1));
    ASSERT_TRUE(right.postFilter(bus, 0, 1));
    ASSERT_FALSE(left.postFilter(bus, 1, 2));
    ASSERT_TRUE(right.postFilter(bus, 1, 2));
    ASSERT_FALSE(left.postFilter(bus, 2, 3));
    ASSERT_FALSE(right.postFilter(bus, 2, 3));
    ASSERT_TRUE(left.postFilter(bus, 3, 4));
    ASSERT_FALSE(right.postFilter(bus, 3, 4));
  }

  FilterByValue left;
  FilterByValue right;
  ComboFilter combo;
  AssertableBuffer buffer;
  RS485Bus<8> bus;
};

TEST_F(ComboFilterTest, by_default_combo_is_enabled_if_both_inputs_are_enabled) {
  ASSERT_TRUE(left.isEnabled());
  ASSERT_TRUE(right.isEnabled());
  ASSERT_TRUE(combo.isEnabled());

  left.setEnabled(false);
  ASSERT_TRUE(combo.isEnabled());

  left.setEnabled();
  right.setEnabled(false);
  ASSERT_TRUE(combo.isEnabled());

  left.setEnabled(false);
  ASSERT_FALSE(combo.isEnabled());
}

TEST_F(ComboFilterTest, can_disable_combo_filter_regardless_of_input_states) {
  ASSERT_TRUE(left.isEnabled());
  ASSERT_TRUE(right.isEnabled());
  ASSERT_TRUE(combo.isEnabled());

  combo.setEnabled(false);

  ASSERT_FALSE(combo.isEnabled());
}

TEST_F(ComboFilterTest, look_ahead_bytes_are_the_max_of_the_two_inputs) {
  ASSERT_EQ(1, combo.lookAheadBytes());
}

TEST_F(ComboFilterTest, by_default_both_filters_must_be_valid_and_enabled) {
  ASSERT_TRUE(combo.preFilter(bus, 0));
  ASSERT_TRUE(combo.postFilter(bus, 0, 1));
  ASSERT_FALSE(combo.preFilter(bus, 1));
  ASSERT_FALSE(combo.postFilter(bus, 1, 2));
  ASSERT_FALSE(combo.preFilter(bus, 2));
  ASSERT_FALSE(combo.postFilter(bus, 2, 3));
  ASSERT_FALSE(combo.preFilter(bus, 3));
  ASSERT_FALSE(combo.postFilter(bus, 3, 4));

  left.setEnabled(false);
  ASSERT_FALSE(combo.preFilter(bus, 0));
  ASSERT_FALSE(combo.postFilter(bus, 0, 1));
  ASSERT_FALSE(combo.preFilter(bus, 1));
  ASSERT_FALSE(combo.postFilter(bus, 1, 2));
  ASSERT_FALSE(combo.preFilter(bus, 2));
  ASSERT_FALSE(combo.postFilter(bus, 2, 3));
  ASSERT_FALSE(combo.preFilter(bus, 3));
  ASSERT_FALSE(combo.postFilter(bus, 3, 4));

  left.setEnabled();
  right.setEnabled(false);
  ASSERT_FALSE(combo.preFilter(bus, 0));
  ASSERT_FALSE(combo.postFilter(bus, 0, 1));
  ASSERT_FALSE(combo.preFilter(bus, 1));
  ASSERT_FALSE(combo.postFilter(bus, 1, 2));
  ASSERT_FALSE(combo.preFilter(bus, 2));
  ASSERT_FALSE(combo.postFilter(bus, 2, 3));
  ASSERT_FALSE(combo.preFilter(bus, 3));
  ASSERT_FALSE(combo.postFilter(bus, 3, 4));

  left.setEnabled(false);
  ASSERT_FALSE(combo.preFilter(bus, 0));
  ASSERT_FALSE(combo.postFilter(bus, 0, 1));
  ASSERT_FALSE(combo.preFilter(bus, 1));
  ASSERT_FALSE(combo.postFilter(bus, 1, 2));
  ASSERT_FALSE(combo.preFilter(bus, 2));
  ASSERT_FALSE(combo.postFilter(bus, 2, 3));
  ASSERT_FALSE(combo.preFilter(bus, 3));
  ASSERT_FALSE(combo.postFilter(bus, 3, 4));
}

TEST_F(ComboFilterTest, either_filter_must_be_valid) {
  combo.preFilterType = ComboFilterType::EITHER_FILTER_MUST_BE_VALID;
  combo.postFilterType = ComboFilterType::EITHER_FILTER_MUST_BE_VALID;

  ASSERT_TRUE(combo.preFilter(bus, 0));
  ASSERT_TRUE(combo.postFilter(bus, 0, 1));
  ASSERT_TRUE(combo.preFilter(bus, 1));
  ASSERT_TRUE(combo.postFilter(bus, 1, 2));
  ASSERT_FALSE(combo.preFilter(bus, 2));
  ASSERT_FALSE(combo.postFilter(bus, 2, 3));
  ASSERT_TRUE(combo.preFilter(bus, 3));
  ASSERT_TRUE(combo.postFilter(bus, 3, 4));

  left.setEnabled(false);
  ASSERT_TRUE(combo.preFilter(bus, 0));
  ASSERT_TRUE(combo.postFilter(bus, 0, 1));
  ASSERT_TRUE(combo.preFilter(bus, 1));
  ASSERT_TRUE(combo.postFilter(bus, 1, 2));
  ASSERT_FALSE(combo.preFilter(bus, 2));
  ASSERT_FALSE(combo.postFilter(bus, 2, 3));
  ASSERT_FALSE(combo.preFilter(bus, 3));
  ASSERT_FALSE(combo.postFilter(bus, 3, 4));

  left.setEnabled();
  right.setEnabled(false);
  ASSERT_TRUE(combo.preFilter(bus, 0));
  ASSERT_TRUE(combo.postFilter(bus, 0, 1));
  ASSERT_FALSE(combo.preFilter(bus, 1));
  ASSERT_FALSE(combo.postFilter(bus, 1, 2));
  ASSERT_FALSE(combo.preFilter(bus, 2));
  ASSERT_FALSE(combo.postFilter(bus, 2, 3));
  ASSERT_TRUE(combo.preFilter(bus, 3));
  ASSERT_TRUE(combo.postFilter(bus, 3, 4));

  left.setEnabled(false);
  ASSERT_FALSE(combo.preFilter(bus, 0));
  ASSERT_FALSE(combo.postFilter(bus, 0, 1));
  ASSERT_FALSE(combo.preFilter(bus, 1));
  ASSERT_FALSE(combo.postFilter(bus, 1, 2));
  ASSERT_FALSE(combo.preFilter(bus, 2));
  ASSERT_FALSE(combo.postFilter(bus, 2, 3));
  ASSERT_FALSE(combo.preFilter(bus, 3));
  ASSERT_FALSE(combo.postFilter(bus, 3, 4));
}

TEST_F(ComboFilterTest, left_ignored) {
  combo.preFilterType = ComboFilterType::LEFT_IGNORED;
  combo.postFilterType = ComboFilterType::LEFT_IGNORED;

  ASSERT_TRUE(combo.preFilter(bus, 0));
  ASSERT_TRUE(combo.postFilter(bus, 0, 1));
  ASSERT_TRUE(combo.preFilter(bus, 1));
  ASSERT_TRUE(combo.postFilter(bus, 1, 2));
  ASSERT_FALSE(combo.preFilter(bus, 2));
  ASSERT_FALSE(combo.postFilter(bus, 2, 3));
  ASSERT_FALSE(combo.preFilter(bus, 3));
  ASSERT_FALSE(combo.postFilter(bus, 3, 4));

  left.setEnabled(false);
  ASSERT_TRUE(combo.preFilter(bus, 0));
  ASSERT_TRUE(combo.postFilter(bus, 0, 1));
  ASSERT_TRUE(combo.preFilter(bus, 1));
  ASSERT_TRUE(combo.postFilter(bus, 1, 2));
  ASSERT_FALSE(combo.preFilter(bus, 2));
  ASSERT_FALSE(combo.postFilter(bus, 2, 3));
  ASSERT_FALSE(combo.preFilter(bus, 3));
  ASSERT_FALSE(combo.postFilter(bus, 3, 4));

  left.setEnabled();
  right.setEnabled(false);
  ASSERT_FALSE(combo.preFilter(bus, 0));
  ASSERT_FALSE(combo.postFilter(bus, 0, 1));
  ASSERT_FALSE(combo.preFilter(bus, 1));
  ASSERT_FALSE(combo.postFilter(bus, 1, 2));
  ASSERT_FALSE(combo.preFilter(bus, 2));
  ASSERT_FALSE(combo.postFilter(bus, 2, 3));
  ASSERT_FALSE(combo.preFilter(bus, 3));
  ASSERT_FALSE(combo.postFilter(bus, 3, 4));

  left.setEnabled(false);
  ASSERT_FALSE(combo.preFilter(bus, 0));
  ASSERT_FALSE(combo.postFilter(bus, 0, 1));
  ASSERT_FALSE(combo.preFilter(bus, 1));
  ASSERT_FALSE(combo.postFilter(bus, 1, 2));
  ASSERT_FALSE(combo.preFilter(bus, 2));
  ASSERT_FALSE(combo.postFilter(bus, 2, 3));
  ASSERT_FALSE(combo.preFilter(bus, 3));
  ASSERT_FALSE(combo.postFilter(bus, 3, 4));
}

TEST_F(ComboFilterTest, right_ignored) {
  combo.preFilterType = ComboFilterType::RIGHT_IGNORED;
  combo.postFilterType = ComboFilterType::RIGHT_IGNORED;

  ASSERT_TRUE(combo.preFilter(bus, 0));
  ASSERT_TRUE(combo.postFilter(bus, 0, 1));
  ASSERT_FALSE(combo.preFilter(bus, 1));
  ASSERT_FALSE(combo.postFilter(bus, 1, 2));
  ASSERT_FALSE(combo.preFilter(bus, 2));
  ASSERT_FALSE(combo.postFilter(bus, 2, 3));
  ASSERT_TRUE(combo.preFilter(bus, 3));
  ASSERT_TRUE(combo.postFilter(bus, 3, 4));

  left.setEnabled(false);
  ASSERT_FALSE(combo.preFilter(bus, 0));
  ASSERT_FALSE(combo.postFilter(bus, 0, 1));
  ASSERT_FALSE(combo.preFilter(bus, 1));
  ASSERT_FALSE(combo.postFilter(bus, 1, 2));
  ASSERT_FALSE(combo.preFilter(bus, 2));
  ASSERT_FALSE(combo.postFilter(bus, 2, 3));
  ASSERT_FALSE(combo.preFilter(bus, 3));
  ASSERT_FALSE(combo.postFilter(bus, 3, 4));

  left.setEnabled();
  right.setEnabled(false);
  ASSERT_TRUE(combo.preFilter(bus, 0));
  ASSERT_TRUE(combo.postFilter(bus, 0, 1));
  ASSERT_FALSE(combo.preFilter(bus, 1));
  ASSERT_FALSE(combo.postFilter(bus, 1, 2));
  ASSERT_FALSE(combo.preFilter(bus, 2));
  ASSERT_FALSE(combo.postFilter(bus, 2, 3));
  ASSERT_TRUE(combo.preFilter(bus, 3));
  ASSERT_TRUE(combo.postFilter(bus, 3, 4));

  left.setEnabled(false);
  ASSERT_FALSE(combo.preFilter(bus, 0));
  ASSERT_FALSE(combo.postFilter(bus, 0, 1));
  ASSERT_FALSE(combo.preFilter(bus, 1));
  ASSERT_FALSE(combo.postFilter(bus, 1, 2));
  ASSERT_FALSE(combo.preFilter(bus, 2));
  ASSERT_FALSE(combo.postFilter(bus, 2, 3));
  ASSERT_FALSE(combo.preFilter(bus, 3));
  ASSERT_FALSE(combo.postFilter(bus, 3, 4));
}