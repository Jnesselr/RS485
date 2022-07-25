#include <unity.h>

#include "assertable_buffer.h"
#include "rs485bus.hpp"
#include <ArduinoFake.h>

using namespace fakeit;

namespace RS485BusTest {
  const uint8_t readEnablePin = 13;
  const uint8_t writeEnablePin = 14;

  void setUpArduinoFake() {
    When(Method(ArduinoFake(), pinMode)).AlwaysReturn();
    When(Method(ArduinoFake(), digitalWrite)).AlwaysReturn();
  }

  void verifyRS485BusStartupArduinoFake() {
    Verify(Method(ArduinoFake(), pinMode).Using(readEnablePin, OUTPUT)).Once();
    Verify(Method(ArduinoFake(), pinMode).Using(writeEnablePin, OUTPUT)).Once();
    Verify(Method(ArduinoFake(), digitalWrite).Using(readEnablePin, LOW)).Once();
    Verify(Method(ArduinoFake(), digitalWrite).Using(writeEnablePin, LOW)).Once();
  }

  void test_buffer_size_is_template_parameter() {
    AssertableBuffer buffer;

    setUpArduinoFake();

    RS485Bus<8> bus8(buffer, readEnablePin, writeEnablePin);
    TEST_ASSERT_EQUAL_INT(8, bus8.bufferSize());

    RS485Bus<128> bus128(buffer, readEnablePin, writeEnablePin);
    TEST_ASSERT_EQUAL_INT(128, bus128.bufferSize());
  }

  void test_by_default_available_returns_0() {
    AssertableBuffer buffer;

    setUpArduinoFake();

    RS485Bus<8> bus(buffer, readEnablePin, writeEnablePin);

    TEST_ASSERT_EQUAL_INT(0, bus.available());
  }

  void test_empty_buffer_does_nothing() {
    AssertableBuffer buffer;

    setUpArduinoFake();

    RS485Bus<8> bus(buffer, readEnablePin, writeEnablePin);

    bus.fetch();

    TEST_ASSERT_EQUAL_INT(0, bus.available());
    TEST_ASSERT_EQUAL_INT(0, buffer.available());
  }

  void test_can_fetch_available_bytes() {
    AssertableBuffer buffer;

    setUpArduinoFake();

    buffer << 1 << 2 << 3;
    RS485Bus<8> bus(buffer, readEnablePin, writeEnablePin);

    TEST_ASSERT_EQUAL_INT(0, bus.available());
    
    TEST_ASSERT_EQUAL_INT(3, bus.fetch());
    TEST_ASSERT_EQUAL_INT(3, bus.available());
    TEST_ASSERT_EQUAL_INT(0, buffer.available());
    TEST_ASSERT_EQUAL_INT(1, bus[0]);
    TEST_ASSERT_EQUAL_INT(2, bus[1]);
    TEST_ASSERT_EQUAL_INT(3, bus[2]);
    TEST_ASSERT_EQUAL_INT(-1, bus[3]);

    // Read another byte in to the read/write buffer, but don't fetch it
    buffer << 4;
    TEST_ASSERT_EQUAL_INT(3, bus.available());
    TEST_ASSERT_EQUAL_INT(1, buffer.available());
    TEST_ASSERT_EQUAL_INT(1, bus[0]);
    TEST_ASSERT_EQUAL_INT(2, bus[1]);
    TEST_ASSERT_EQUAL_INT(3, bus[2]);
    TEST_ASSERT_EQUAL_INT(-1, bus[3]);

    TEST_ASSERT_EQUAL(1, bus.fetch());
    TEST_ASSERT_EQUAL_INT(1, bus[0]);
    TEST_ASSERT_EQUAL_INT(2, bus[1]);
    TEST_ASSERT_EQUAL_INT(3, bus[2]);
    TEST_ASSERT_EQUAL_INT(4, bus[3]);
    TEST_ASSERT_EQUAL_INT(-1, bus[4]);
  }

  void test_will_not_fetch_bytes_if_internal_buffer_is_full() {
    AssertableBuffer buffer;

    setUpArduinoFake();

    buffer << 1 << 2 << 3;
    RS485Bus<2> bus(buffer, readEnablePin, writeEnablePin);

    TEST_ASSERT_EQUAL_INT(3, buffer.available());
    TEST_ASSERT_EQUAL_INT(0, bus.available());

    bus.fetch();

    TEST_ASSERT_EQUAL_INT(1, buffer.available());
    TEST_ASSERT_EQUAL_INT(2, bus.available());

    TEST_ASSERT_EQUAL_INT(1, bus[0]);
    TEST_ASSERT_EQUAL_INT(2, bus[1]);
    TEST_ASSERT_EQUAL_INT(-1, bus[2]);

    TEST_ASSERT_EQUAL_INT(3, buffer[0]);
    TEST_ASSERT_EQUAL_INT(-1, buffer[1]);
  }

  void test_by_default_read_pin_is_enabled_and_write_is_disabled() {
    AssertableBuffer buffer;

    setUpArduinoFake();

    RS485Bus<8> bus(buffer, readEnablePin, writeEnablePin);

    verifyRS485BusStartupArduinoFake();
  }

  void test_writing_a_byte_verifies_it_was_written() {
    AssertableBuffer buffer;
    Mock<AssertableBuffer> spy = buffer.spy();

    setUpArduinoFake();

    RS485Bus<8> bus(buffer, readEnablePin, writeEnablePin);

    verifyRS485BusStartupArduinoFake();

    buffer << 0x37;  // Next byte to be read
    TEST_ASSERT_EQUAL_INT(WriteStatus::OK, bus.write(0x37));

    Verify(
      Method(ArduinoFake(), digitalWrite).Using(writeEnablePin, HIGH),
      Method(spy, write).Using(0x37),
      Method(ArduinoFake(), digitalWrite).Using(writeEnablePin, LOW),
      Method(spy, available),
      Method(spy, read)
    ).Once();
  }

  void test_writing_a_byte_when_a_different_one_is_read() {
    AssertableBuffer buffer;
    Mock<AssertableBuffer> spy = buffer.spy();

    setUpArduinoFake();

    RS485Bus<8> bus(buffer, readEnablePin, writeEnablePin);

    verifyRS485BusStartupArduinoFake();
    When(Method(ArduinoFake(), delay)).AlwaysReturn();

    buffer << 0x21;  // Next byte to be read
    TEST_ASSERT_EQUAL_INT(WriteStatus::FAILED_READ_BACK, bus.write(0x37));

    Verify(
      Method(ArduinoFake(), digitalWrite).Using(writeEnablePin, HIGH),
      Method(spy, write).Using(0x37),
      Method(ArduinoFake(), digitalWrite).Using(writeEnablePin, LOW),
      Method(spy, available),
      Method(spy, read)
    ).Once();
  }

  void test_writing_a_byte_when_a_different_one_is_read_first() {
    AssertableBuffer buffer;
    Mock<AssertableBuffer> spy = buffer.spy();

    setUpArduinoFake();

    RS485Bus<8> bus(buffer, readEnablePin, writeEnablePin);

    verifyRS485BusStartupArduinoFake();

    buffer << 0x21 << 0x05 << 0x37;  // Next byte to be read
    TEST_ASSERT_EQUAL_INT(WriteStatus::UNEXPECTED_EXTRA_BYTES, bus.write(0x37));

    Verify(
      Method(ArduinoFake(), digitalWrite).Using(writeEnablePin, HIGH),
      Method(spy, write).Using(0x37),
      Method(ArduinoFake(), digitalWrite).Using(writeEnablePin, LOW),
      Method(spy, available),
      Method(spy, read),
      Method(spy, read),
      Method(spy, read)
    ).Once();

    TEST_ASSERT_EQUAL_INT(0x21, bus[0]);
    TEST_ASSERT_EQUAL_INT(0x05, bus[1]);
    TEST_ASSERT_EQUAL_INT(-1, bus[2]);
  }

  void test_writing_a_byte_when_no_new_byte_is_available() {
    AssertableBuffer buffer;
    Mock<AssertableBuffer> spy = buffer.spy();

    setUpArduinoFake();

    RS485Bus<8> bus(buffer, readEnablePin, writeEnablePin);
    bus.setReadBackRetries(0);
    bus.setReadBackDelayMs(2);

    verifyRS485BusStartupArduinoFake();

    TEST_ASSERT_EQUAL_INT(WriteStatus::NO_READ_TIMEOUT, bus.write(0x37));

    Verify(
      Method(ArduinoFake(), digitalWrite).Using(writeEnablePin, HIGH),
      Method(spy, write).Using(0x37),
      Method(ArduinoFake(), digitalWrite).Using(writeEnablePin, LOW),
      Method(spy, available)
    ).Once();
  }

  void test_writing_a_byte_when_no_new_byte_is_available_with_delays() {
    AssertableBuffer buffer;
    Mock<AssertableBuffer> spy = buffer.spy();

    setUpArduinoFake();

    RS485Bus<8> bus(buffer, readEnablePin, writeEnablePin);
    bus.setReadBackDelayMs(5);
    bus.setReadBackRetries(3);

    verifyRS485BusStartupArduinoFake();

    When(Method(ArduinoFake(), delay)).AlwaysReturn();

    TEST_ASSERT_EQUAL_INT(WriteStatus::NO_READ_TIMEOUT, bus.write(0x37));

    Verify(
      Method(ArduinoFake(), digitalWrite).Using(writeEnablePin, HIGH),
      Method(spy, write).Using(0x37),
      Method(ArduinoFake(), digitalWrite).Using(writeEnablePin, LOW),
      Method(spy, available),
      Method(ArduinoFake(), delay).Using(5),
      Method(spy, available),
      Method(ArduinoFake(), delay).Using(5),
      Method(spy, available),
      Method(ArduinoFake(), delay).Using(5),
      Method(spy, available)
    ).Once();
  }

  /**
   * This method is kinda weird because we also test that the retry counter gets reset when
   * a byte is read, even if that byte is incorrect.
   */
  void test_writing_a_byte_that_eventually_returns_correct_value() {
    AssertableBuffer buffer;
    Mock<AssertableBuffer> spy = buffer.spy();

    setUpArduinoFake();

    buffer << 0x21 << 0x37;
    RS485Bus<8> bus(buffer, readEnablePin, writeEnablePin);
    bus.setReadBackDelayMs(7);
    bus.setReadBackRetries(1);

    verifyRS485BusStartupArduinoFake();

    When(Method(ArduinoFake(), delay)).AlwaysReturn();
    When(Method(spy, available)).Return(0, 1, 0, 1);

    TEST_ASSERT_EQUAL_INT(WriteStatus::UNEXPECTED_EXTRA_BYTES, bus.write(0x37));

    Verify(
      Method(ArduinoFake(), digitalWrite).Using(writeEnablePin, HIGH),
      Method(spy, write).Using(0x37),
      Method(ArduinoFake(), digitalWrite).Using(writeEnablePin, LOW),
      Method(spy, available),
      Method(ArduinoFake(), delay).Using(7),
      Method(spy, available),
      Method(spy, read),
      Method(spy, available),
      Method(ArduinoFake(), delay).Using(7),
      Method(spy, available),
      Method(spy, read)
    ).Once();

    TEST_ASSERT_EQUAL_INT(0x21, bus[0]);
    TEST_ASSERT_EQUAL_INT(-1, bus[1]);
  }

  void test_write_when_buffer_becomes_full() {
    AssertableBuffer buffer;
    Mock<AssertableBuffer> spy = buffer.spy();

    setUpArduinoFake();

    buffer << 0x21 << 0x30 << 0x37;
    RS485Bus<2> bus(buffer, readEnablePin, writeEnablePin);
    bus.setReadBackRetries(0);

    verifyRS485BusStartupArduinoFake();

    When(Method(ArduinoFake(), delay)).AlwaysReturn();

    TEST_ASSERT_EQUAL_INT(WriteStatus::READ_BUFFER_FULL, bus.write(0x37));

    Verify(
      Method(ArduinoFake(), digitalWrite).Using(writeEnablePin, HIGH),
      Method(spy, write).Using(0x37),
      Method(ArduinoFake(), digitalWrite).Using(writeEnablePin, LOW),
      Method(spy, available),
      Method(spy, read),
      Method(spy, available),
      Method(spy, read),
      Method(spy, available)
    ).Once();

    TEST_ASSERT_EQUAL_INT(0x21, bus[0]);
    TEST_ASSERT_EQUAL_INT(0x30, bus[1]);
    TEST_ASSERT_EQUAL_INT(-1, bus[2]);
  }

  void test_write_when_buffer_becomes_full_but_no_more_is_available() {
    AssertableBuffer buffer;
    Mock<AssertableBuffer> spy = buffer.spy();

    setUpArduinoFake();

    buffer << 0x21 << 0x30;
    RS485Bus<2> bus(buffer, readEnablePin, writeEnablePin);
    bus.setReadBackRetries(0);

    verifyRS485BusStartupArduinoFake();

    When(Method(ArduinoFake(), delay)).AlwaysReturn();

    TEST_ASSERT_EQUAL_INT(WriteStatus::FAILED_READ_BACK, bus.write(0x37));

    Verify(
      Method(ArduinoFake(), digitalWrite).Using(writeEnablePin, HIGH),
      Method(spy, write).Using(0x37),
      Method(ArduinoFake(), digitalWrite).Using(writeEnablePin, LOW),
      Method(spy, available),
      Method(spy, read),
      Method(spy, available),
      Method(spy, read),
      Method(spy, available)
    ).Once();

    TEST_ASSERT_EQUAL_INT(0x21, bus[0]);
    TEST_ASSERT_EQUAL_INT(0x30, bus[1]);
    TEST_ASSERT_EQUAL_INT(-1, bus[2]);
  }

  // TODO Can we test for the weird edge case of seeing a byte before we expect it and it's the same as ours but not ours?

  void run_tests() {
    RUN_TEST(test_buffer_size_is_template_parameter);
    RUN_TEST(test_by_default_available_returns_0);
    RUN_TEST(test_empty_buffer_does_nothing);
    RUN_TEST(test_can_fetch_available_bytes);
    RUN_TEST(test_will_not_fetch_bytes_if_internal_buffer_is_full);
    RUN_TEST(test_by_default_read_pin_is_enabled_and_write_is_disabled);
    RUN_TEST(test_writing_a_byte_verifies_it_was_written);
    RUN_TEST(test_writing_a_byte_when_a_different_one_is_read);
    RUN_TEST(test_writing_a_byte_when_a_different_one_is_read_first);
    RUN_TEST(test_writing_a_byte_when_no_new_byte_is_available);
    RUN_TEST(test_writing_a_byte_when_no_new_byte_is_available_with_delays);
    RUN_TEST(test_writing_a_byte_that_eventually_returns_correct_value);
    RUN_TEST(test_write_when_buffer_becomes_full);
    RUN_TEST(test_write_when_buffer_becomes_full_but_no_more_is_available);
  }
}