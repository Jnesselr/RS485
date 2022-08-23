#pragma once

#include <gtest/gtest.h>
#include "../assertable_buffer.h"

class AssertableBufferTest : public ::testing::Test {
protected:
  AssertableBuffer buffer;
};

TEST_F(AssertableBufferTest, by_default_available_is_0) {
  EXPECT_EQ(buffer.available(), 0);
}

TEST_F(AssertableBufferTest, reading_with_no_bytes_readable_returns_negative_1) {
  EXPECT_EQ(buffer.read(), -1);
}

TEST_F(AssertableBufferTest, bytes_are_inspectable) {
  buffer << 1 << 2 << 3;

  EXPECT_EQ(buffer.available(), 3);
  EXPECT_EQ(buffer[0], 1);
  EXPECT_EQ(buffer[1], 2);
  EXPECT_EQ(buffer[2], 3);
  EXPECT_EQ(buffer[3], -1);
}

TEST_F(AssertableBufferTest, reading_bytes_changes_available) {
  buffer << 1 << 2 << 3;

  EXPECT_EQ(buffer.read(), 1);

  EXPECT_EQ(buffer.available(), 2);
  EXPECT_EQ(buffer[0], 2);
  EXPECT_EQ(buffer[1], 3);
  EXPECT_EQ(buffer[2], -1);
}

TEST_F(AssertableBufferTest, asserting_written_bytes) {
  buffer.write(1);
  buffer.write(2);
  buffer.write(3);

  EXPECT_EQ(buffer.available(), 0);

  EXPECT_EQ(buffer.written(), 1);
  EXPECT_EQ(buffer.written(), 2);
  EXPECT_EQ(buffer.written(), 3);
  EXPECT_EQ(buffer.written(), -1);
}