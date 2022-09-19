#pragma once

#include <gtest/gtest.h>
#include "../assertable_bus_io.hpp"

class AssertableBusIOTest : public ::testing::Test {
protected:
  AssertableBusIO busIO;
};

TEST_F(AssertableBusIOTest, by_default_available_is_0) {
  EXPECT_EQ(0, busIO.available());
}

TEST_F(AssertableBusIOTest, reading_with_no_bytes_readable_returns_negative_1) {
  EXPECT_EQ(-1, busIO.read());
}

TEST_F(AssertableBusIOTest, bytes_are_inspectable) {
  busIO << 1 << 2 << 3;

  EXPECT_EQ(3, busIO.available());
  EXPECT_EQ(1, busIO[0]);
  EXPECT_EQ(2, busIO[1]);
  EXPECT_EQ(3, busIO[2]);
  EXPECT_EQ(-1, busIO[3]);
}

TEST_F(AssertableBusIOTest, reading_bytes_changes_available) {
  busIO << 1 << 2 << 3;

  EXPECT_EQ(1, busIO.read());

  EXPECT_EQ(2, busIO.available());
  EXPECT_EQ(2, busIO[0]);
  EXPECT_EQ(3, busIO[1]);
  EXPECT_EQ(-1, busIO[2]);
}

TEST_F(AssertableBusIOTest, asserting_written_bytes) {
  busIO.write(1);
  busIO.write(2);
  busIO.write(3);

  EXPECT_EQ(0, busIO.available());

  EXPECT_EQ(1, busIO.written());
  EXPECT_EQ(2, busIO.written());
  EXPECT_EQ(3, busIO.written());
  EXPECT_EQ(-1, busIO.written());
}