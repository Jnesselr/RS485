#include <unity.h>

#include "test_buffer.h"

void test_by_default_available_is_0() {
  TestBuffer buffer;

  TEST_ASSERT_EQUAL_INT(0, buffer.available());
}

void test_reading_with_no_bytes_readable_returns_negative_1() {
  TestBuffer buffer;

  TEST_ASSERT_EQUAL_INT(-1, buffer.read());
}

void test_making_bytes_readable() {
  TestBuffer buffer;
  buffer << 1 << 2 << 3;

  TEST_ASSERT_EQUAL_INT(3, buffer.available());
  TEST_ASSERT_EQUAL_INT(1, buffer[0]);
  TEST_ASSERT_EQUAL_INT(2, buffer[1]);
  TEST_ASSERT_EQUAL_INT(3, buffer[2]);
}

void test_reading_bytes_changes_available() {
  TestBuffer buffer;
  buffer << 1 << 2 << 3;

  TEST_ASSERT_EQUAL_INT(1, buffer.read());

  TEST_ASSERT_EQUAL_INT(2, buffer.available());
  TEST_ASSERT_EQUAL_INT(2, buffer[0]);
  TEST_ASSERT_EQUAL_INT(3, buffer[1]);
}

void test_asserting_written_bytes() {
  TestBuffer buffer;
  buffer.write(1);
  buffer.write(2);
  buffer.write(3);

  TEST_ASSERT_EQUAL_INT(0, buffer.available());

  TEST_ASSERT_EQUAL_INT(1, buffer.written());
  TEST_ASSERT_EQUAL_INT(2, buffer.written());
  TEST_ASSERT_EQUAL_INT(3, buffer.written());
  TEST_ASSERT_EQUAL_INT(-1, buffer.written());
}


int main(int argc, char **argv) {
  UNITY_BEGIN();

  RUN_TEST(test_by_default_available_is_0);
  RUN_TEST(test_reading_with_no_bytes_readable_returns_negative_1);
  RUN_TEST(test_making_bytes_readable);
  RUN_TEST(test_reading_bytes_changes_available);
  RUN_TEST(test_asserting_written_bytes);

  UNITY_END();
}