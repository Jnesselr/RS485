#pragma once

#include <gtest/gtest.h>
#include "../assertable_buffer.h"

class AssertableBufferTest : public ::testing::Test {
protected:
  AssertableBuffer buffer;
};

TEST_F(AssertableBufferTest, by_default_available_is_0) {
  EXPECT_EQ(0, buffer.available());
}

TEST_F(AssertableBufferTest, reading_with_no_bytes_readable_returns_negative_1) {
  EXPECT_EQ(-1, buffer.read());
}

TEST_F(AssertableBufferTest, bytes_are_inspectable) {
  buffer << 1 << 2 << 3;

  EXPECT_EQ(3, buffer.available());
  EXPECT_EQ(1, buffer[0]);
  EXPECT_EQ(2, buffer[1]);
  EXPECT_EQ(3, buffer[2]);
  EXPECT_EQ(-1, buffer[3]);
}

TEST_F(AssertableBufferTest, reading_bytes_changes_available) {
  buffer << 1 << 2 << 3;

  EXPECT_EQ(1, buffer.read());

  EXPECT_EQ(2, buffer.available());
  EXPECT_EQ(2, buffer[0]);
  EXPECT_EQ(3, buffer[1]);
  EXPECT_EQ(-1, buffer[2]);
}

TEST_F(AssertableBufferTest, asserting_written_bytes) {
  buffer.write(1);
  buffer.write(2);
  buffer.write(3);

  EXPECT_EQ(0, buffer.available());

  EXPECT_EQ(1, buffer.written());
  EXPECT_EQ(2, buffer.written());
  EXPECT_EQ(3, buffer.written());
  EXPECT_EQ(-1, buffer.written());
}