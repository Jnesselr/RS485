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
    When(Method(spy, available)).Return(0, 1);

    TEST_ASSERT_EQUAL_INT(WriteStatus::OK, bus.write(0x37));

    Verify(
      Method(spy, available),
      Method(ArduinoFake(), digitalWrite).Using(writeEnablePin, HIGH),
      Method(spy, write).Using(0x37),
      Method(ArduinoFake(), digitalWrite).Using(writeEnablePin, LOW),
      Method(spy, available),
      Method(spy, read)
    ).Once();

    VerifyNoOtherInvocations(spy);
  }

  void test_writing_a_byte_when_a_different_one_is_read() {
    AssertableBuffer buffer;
    Mock<AssertableBuffer> spy = buffer.spy();

    setUpArduinoFake();

    RS485Bus<8> bus(buffer, readEnablePin, writeEnablePin);

    verifyRS485BusStartupArduinoFake();
    When(Method(ArduinoFake(), delay)).AlwaysReturn();

    buffer << 0x21;  // Next byte to be read
    When(Method(spy, available)).Return(0, 1, 0);

    TEST_ASSERT_EQUAL_INT(WriteStatus::FAILED_READ_BACK, bus.write(0x37));

    Verify(
      Method(spy, available),
      Method(ArduinoFake(), digitalWrite).Using(writeEnablePin, HIGH),
      Method(spy, write).Using(0x37),
      Method(ArduinoFake(), digitalWrite).Using(writeEnablePin, LOW),
      Method(spy, available),
      Method(spy, read),
      Method(spy, available)
    ).Once();

    VerifyNoOtherInvocations(spy);
  }

  void test_writing_a_byte_when_a_different_one_is_read_first() {
    AssertableBuffer buffer;
    Mock<AssertableBuffer> spy = buffer.spy();

    setUpArduinoFake();

    RS485Bus<8> bus(buffer, readEnablePin, writeEnablePin);

    verifyRS485BusStartupArduinoFake();

    buffer << 0x21 << 0x05 << 0x37;  // Next byte to be read
    When(Method(spy, available)).Return(0, 3, 2, 1, 0);

    TEST_ASSERT_EQUAL_INT(WriteStatus::UNEXPECTED_EXTRA_BYTES, bus.write(0x37));

    Verify(
      Method(spy, available), // Returns 0
      Method(ArduinoFake(), digitalWrite).Using(writeEnablePin, HIGH),
      Method(spy, write).Using(0x37),
      Method(ArduinoFake(), digitalWrite).Using(writeEnablePin, LOW),
      Method(spy, available), // Returns 3
      Method(spy, read),
      Method(spy, available), // Returns 2
      Method(spy, read),
      Method(spy, available), // Returns 1
      Method(spy, read)
    ).Once();

    VerifyNoOtherInvocations(spy);

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
      Method(spy, available),
      Method(ArduinoFake(), digitalWrite).Using(writeEnablePin, HIGH),
      Method(spy, write).Using(0x37),
      Method(ArduinoFake(), digitalWrite).Using(writeEnablePin, LOW),
      Method(spy, available)
    ).Once();

    VerifyNoOtherInvocations(spy);
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
      Method(spy, available),
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

    VerifyNoOtherInvocations(spy);
  }

  /**
   * This method is kinda weird because we also test that the retry counter gets reset when
   * a byte is read, even if that byte is incorrect.
   */
  void test_writing_a_byte_that_eventually_returns_correct_value() {
    AssertableBuffer buffer;
    Mock<AssertableBuffer> spy = buffer.spy();

    setUpArduinoFake();

    RS485Bus<8> bus(buffer, readEnablePin, writeEnablePin);
    bus.setReadBackDelayMs(7);
    bus.setReadBackRetries(1);

    verifyRS485BusStartupArduinoFake();

    When(Method(ArduinoFake(), delay)).AlwaysReturn();

    buffer << 0x21 << 0x37;
    When(Method(spy, available)).Return(0, 0, 1, 0, 1);

    TEST_ASSERT_EQUAL_INT(WriteStatus::UNEXPECTED_EXTRA_BYTES, bus.write(0x37));

    Verify(
      Method(spy, available), // Returns 0
      Method(ArduinoFake(), digitalWrite).Using(writeEnablePin, HIGH),
      Method(spy, write).Using(0x37),
      Method(ArduinoFake(), digitalWrite).Using(writeEnablePin, LOW),
      Method(spy, available), // Returns 0
      Method(ArduinoFake(), delay).Using(7),
      Method(spy, available), // Returns 1
      Method(spy, read),
      Method(spy, available), // Returns 0
      Method(ArduinoFake(), delay).Using(7),
      Method(spy, available), // Returns 1
      Method(spy, read)
    ).Once();

    VerifyNoOtherInvocations(spy);

    TEST_ASSERT_EQUAL_INT(0x21, bus[0]);
    TEST_ASSERT_EQUAL_INT(-1, bus[1]);
  }

  void test_writing_a_byte_if_that_byte_is_in_that_prefetched_buffer() {
    AssertableBuffer buffer;
    Mock<AssertableBuffer> spy = buffer.spy();

    setUpArduinoFake();

    RS485Bus<8> bus(buffer, readEnablePin, writeEnablePin);

    verifyRS485BusStartupArduinoFake();

    When(Method(ArduinoFake(), delay)).AlwaysReturn();
    buffer << 0x21 << 0x37;
    bus.fetch();
    spy.ClearInvocationHistory();

    TEST_ASSERT_EQUAL_INT(0x21, bus[0]);
    TEST_ASSERT_EQUAL_INT(0x37, bus[1]);
    TEST_ASSERT_EQUAL_INT(-1, bus[2]);

    buffer << 0x37; // What we're about to write
    When(Method(spy, available)).Return(0, 1);

    TEST_ASSERT_EQUAL_INT(WriteStatus::OK, bus.write(0x37));

    Verify(
      Method(spy, available),  // Returns 0
      Method(ArduinoFake(), digitalWrite).Using(writeEnablePin, HIGH),
      Method(spy, write).Using(0x37),
      Method(ArduinoFake(), digitalWrite).Using(writeEnablePin, LOW),
      Method(spy, available),  // Returns 1
      Method(spy, read)
    ).Once();

    VerifyNoOtherInvocations(spy);
  }

  void test_fetching_bytes_before_write_returns_no_write_new_bytes() {
    AssertableBuffer buffer;
    Mock<AssertableBuffer> spy = buffer.spy();

    setUpArduinoFake();

    RS485Bus<8> bus(buffer, readEnablePin, writeEnablePin);
    bus.setPreFetchDelayMs(7);
    bus.setPreFetchRetries(2);

    verifyRS485BusStartupArduinoFake();

    When(Method(ArduinoFake(), delay)).AlwaysReturn();

    buffer << 0x21 << 0x34;
    When(Method(spy, available)).Return(1, 0, 0, 1, 0, 0, 0, 0);

    TEST_ASSERT_EQUAL_INT(WriteStatus::NO_WRITE_NEW_BYTES, bus.write(0x37));

    Verify(
      Method(spy, available), // Returns 1, fetch #1
      Method(spy, read),      // 0x21
      Method(spy, available), // Returns 0, fetch #1
      Method(ArduinoFake(), delay).Using(7),
      Method(spy, available), // Returns 0, fetch #2
      Method(ArduinoFake(), delay).Using(7),
      Method(spy, available), // Returns 1, fetch #3
      Method(spy, read),      // 0x34
      Method(spy, available), // Returns 0, fetch #3
      Method(ArduinoFake(), delay).Using(7),
      Method(spy, available), // Returns 0, fetch #4
      Method(ArduinoFake(), delay).Using(7),
      Method(spy, available), // Returns 0, fetch #5
      Method(ArduinoFake(), delay).Using(7),
      Method(spy, available)  // Returns 0, fetch #6
    ).Once();

    Verify(Method(spy, write)).Never();
    VerifyNoOtherInvocations(spy);
    VerifyNoOtherInvocations(Method(ArduinoFake(), digitalWrite));

    TEST_ASSERT_EQUAL_INT(0x21, bus[0]);
    TEST_ASSERT_EQUAL_INT(0x34, bus[1]);
    TEST_ASSERT_EQUAL_INT(-1, bus[2]);
  }

  void test_write_when_buffer_should_already_be_full() {
    AssertableBuffer buffer;
    Mock<AssertableBuffer> spy = buffer.spy();

    setUpArduinoFake();

    buffer << 0x21 << 0x30 << 0x37;
    RS485Bus<2> bus(buffer, readEnablePin, writeEnablePin);
    bus.setReadBackRetries(0);

    verifyRS485BusStartupArduinoFake();

    When(Method(ArduinoFake(), delay)).AlwaysReturn();

    TEST_ASSERT_EQUAL_INT(WriteStatus::NO_WRITE_BUFFER_FULL, bus.write(0x37));

    Verify(
      Method(spy, available),
      Method(spy, read),
      Method(spy, available),
      Method(spy, read)
    ).Once();

    Verify(Method(spy, write)).Never();
    VerifyNoOtherInvocations(spy);
    VerifyNoOtherInvocations(Method(ArduinoFake(), digitalWrite));

    TEST_ASSERT_EQUAL_INT(0x21, bus[0]);
    TEST_ASSERT_EQUAL_INT(0x30, bus[1]);
    TEST_ASSERT_EQUAL_INT(-1, bus[2]);
    TEST_ASSERT_EQUAL_INT(1, buffer.available()); // 1 byte style available
  }

  /**
   * Simulate what would happen if no bytes appeared available, so we wrote our byte, but then
   * proceeded to see other new bytes that filled the buffer.
   */
  void test_write_when_buffer_becomes_full() {
    AssertableBuffer buffer;
    Mock<AssertableBuffer> spy = buffer.spy();

    setUpArduinoFake();

    buffer << 0x21 << 0x30 << 0x37;
    RS485Bus<2> bus(buffer, readEnablePin, writeEnablePin);
    bus.setReadBackRetries(0);

    verifyRS485BusStartupArduinoFake();

    When(Method(ArduinoFake(), delay)).AlwaysReturn();
    When(Method(spy, available)).Return(0, 3, 2, 1);

    TEST_ASSERT_EQUAL_INT(WriteStatus::READ_BUFFER_FULL, bus.write(0x37));

    Verify(
      Method(spy, available), // Returns 0
      Method(ArduinoFake(), digitalWrite).Using(writeEnablePin, HIGH),
      Method(spy, write).Using(0x37),
      Method(ArduinoFake(), digitalWrite).Using(writeEnablePin, LOW),
      Method(spy, available), // Returns 3
      Method(spy, read),
      Method(spy, available), // Returns 2
      Method(spy, read),
      Method(spy, available) // Returns 1
    ).Once();

    VerifyNoOtherInvocations(spy);

    TEST_ASSERT_EQUAL_INT(0x21, bus[0]);
    TEST_ASSERT_EQUAL_INT(0x30, bus[1]);
    TEST_ASSERT_EQUAL_INT(-1, bus[2]);
    TEST_ASSERT_EQUAL_INT(1, buffer.available()); // 1 byte style available
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
    When(Method(spy, available)).Return(0, 2, 1, 0);

    TEST_ASSERT_EQUAL_INT(WriteStatus::FAILED_READ_BACK, bus.write(0x37));

    Verify(
      Method(spy, available), // Returns 0
      Method(ArduinoFake(), digitalWrite).Using(writeEnablePin, HIGH),
      Method(spy, write).Using(0x37),
      Method(ArduinoFake(), digitalWrite).Using(writeEnablePin, LOW),
      Method(spy, available), // Returns 2
      Method(spy, read),
      Method(spy, available), // Returns 1
      Method(spy, read),
      Method(spy, available) // Returns 0
    ).Once();

    VerifyNoOtherInvocations(spy);

    TEST_ASSERT_EQUAL_INT(0x21, bus[0]);
    TEST_ASSERT_EQUAL_INT(0x30, bus[1]);
    TEST_ASSERT_EQUAL_INT(-1, bus[2]);
  }

  void test_read_calls_fetch() {
    AssertableBuffer buffer;

    setUpArduinoFake();

    buffer << 1 << 2 << 3;
    RS485Bus<8> bus(buffer, readEnablePin, writeEnablePin);

    // We haven't called fetch internally
    TEST_ASSERT_EQUAL_INT(0, bus.available());

    TEST_ASSERT_EQUAL_INT(1, bus.read());
    TEST_ASSERT_EQUAL_INT(2, bus.available());

    TEST_ASSERT_EQUAL_INT(2, bus.read());
    TEST_ASSERT_EQUAL_INT(1, bus.available());

    TEST_ASSERT_EQUAL_INT(3, bus.read());
    TEST_ASSERT_EQUAL_INT(0, bus.available());

    TEST_ASSERT_EQUAL_INT(-1, bus.read());
    TEST_ASSERT_EQUAL_INT(0, bus.available());
  }

  void test_read_fetches_even_if_not_all_bytes_have_been_read() {
    AssertableBuffer buffer;

    setUpArduinoFake();

    buffer << 1;
    RS485Bus<8> bus(buffer, readEnablePin, writeEnablePin);

    bus.fetch();
    buffer << 2 << 3;
    TEST_ASSERT_EQUAL_INT(1, bus.available());

    TEST_ASSERT_EQUAL_INT(1, bus.read());
    TEST_ASSERT_EQUAL_INT(2, bus.available());

    TEST_ASSERT_EQUAL_INT(2, bus.read());
    TEST_ASSERT_EQUAL_INT(1, bus.available());

    TEST_ASSERT_EQUAL_INT(3, bus.read());
    TEST_ASSERT_EQUAL_INT(0, bus.available());

    TEST_ASSERT_EQUAL_INT(-1, bus.read());
    TEST_ASSERT_EQUAL_INT(0, bus.available());
  }

  void test_read_clears_full_flag() {
    AssertableBuffer buffer;

    setUpArduinoFake();

    buffer << 1 << 2;
    RS485Bus<2> bus(buffer, readEnablePin, writeEnablePin);

    bus.fetch();
    TEST_ASSERT_EQUAL_INT(2, bus.available());
    TEST_ASSERT_TRUE(bus.isBufferFull());

    TEST_ASSERT_EQUAL_INT(1, bus.read());
    TEST_ASSERT_EQUAL_INT(1, bus.available());
    TEST_ASSERT_FALSE(bus.isBufferFull());

    TEST_ASSERT_EQUAL_INT(2, bus.read());
    TEST_ASSERT_EQUAL_INT(0, bus.available());
    TEST_ASSERT_FALSE(bus.isBufferFull());

    TEST_ASSERT_EQUAL_INT(-1, bus.read());
    TEST_ASSERT_EQUAL_INT(0, bus.available());
    TEST_ASSERT_FALSE(bus.isBufferFull());
  }

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
    RUN_TEST(test_writing_a_byte_if_that_byte_is_in_that_prefetched_buffer);
    RUN_TEST(test_fetching_bytes_before_write_returns_no_write_new_bytes);
    RUN_TEST(test_write_when_buffer_should_already_be_full);
    RUN_TEST(test_write_when_buffer_becomes_full);
    RUN_TEST(test_write_when_buffer_becomes_full_but_no_more_is_available);
    RUN_TEST(test_read_calls_fetch);
    RUN_TEST(test_read_fetches_even_if_not_all_bytes_have_been_read);
    RUN_TEST(test_read_clears_full_flag);
  }
}