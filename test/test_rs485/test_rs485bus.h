#pragma once

#include "../assertable_buffer.h"
#include "../fixtures.h"
#include "rs485bus.hpp"
#include <ArduinoFake.h>

using namespace fakeit;

class RS485BusTest : public PrepBus {
public:
  RS485BusTest(): PrepBus(),
    bus2(buffer, readEnablePin, writeEnablePin),
    bus8(buffer, readEnablePin, writeEnablePin) {}

  void SetUp() {
    ArduinoFake().ClearInvocationHistory();
  };

  AssertableBuffer buffer;
  Mock<AssertableBuffer> spy = buffer.spy();

  RS485Bus<2> bus2;
  RS485Bus<8> bus8;
};

TEST_F(RS485BusTest, buffer_size_is_template_parameter) {
  EXPECT_EQ(bus2.bufferSize(), 2);
  EXPECT_EQ(bus8.bufferSize(), 8);
}

TEST_F(RS485BusTest, by_default_available_returns_0) {
  EXPECT_EQ(bus8.available(), 0);
}

TEST_F(RS485BusTest, empty_buffer_does_nothing) {
  bus8.fetch();

  EXPECT_EQ(bus8.available(), 0);
  EXPECT_EQ(buffer.available(), 0);
}

TEST_F(RS485BusTest, can_fetch_available_bytes) {
  buffer << 1 << 2 << 3;

  EXPECT_EQ(bus8.available(), 0);

  EXPECT_EQ(bus8.fetch(), 3);
  EXPECT_EQ(bus8.available(), 3);
  EXPECT_EQ(buffer.available(), 0);
  EXPECT_EQ(bus8[0], 1);
  EXPECT_EQ(bus8[1], 2);
  EXPECT_EQ(bus8[2], 3);
  EXPECT_EQ(bus8[3], -1);

  // Read another byte in to the read/write buffer, but don't fetch it
  buffer << 4;
  EXPECT_EQ(bus8.available(), 3);
  EXPECT_EQ(buffer.available(), 1);
  EXPECT_EQ(bus8[0], 1);
  EXPECT_EQ(bus8[1], 2);
  EXPECT_EQ(bus8[2], 3);
  EXPECT_EQ(bus8[3], -1);

  EXPECT_EQ(bus8.fetch(), 1);
  EXPECT_EQ(bus8[0], 1);
  EXPECT_EQ(bus8[1], 2);
  EXPECT_EQ(bus8[2], 3);
  EXPECT_EQ(bus8[3], 4);
  EXPECT_EQ(bus8[4], -1);
}

TEST_F(RS485BusTest, will_not_fetch_bytes_if_internal_buffer_is_full) {
  buffer << 1 << 2 << 3;

  EXPECT_EQ(buffer.available(), 3);
  EXPECT_EQ(bus2.available(), 0);

  bus2.fetch();

  EXPECT_EQ(buffer.available(), 1);
  EXPECT_EQ(bus2.available(), 2);

  EXPECT_EQ(bus2[0], 1);
  EXPECT_EQ(bus2[1], 2);
  EXPECT_EQ(bus2[2], -1);

  EXPECT_EQ(buffer[0], 3);
  EXPECT_EQ(buffer[1], -1);
}

TEST_F(RS485BusTest, by_default_read_pin_is_enabled_and_write_is_disabled) {
  RS485Bus<8> bus(buffer, readEnablePin, writeEnablePin);

  Verify(Method(ArduinoFake(), pinMode).Using(readEnablePin, OUTPUT)).Once();
  Verify(Method(ArduinoFake(), pinMode).Using(writeEnablePin, OUTPUT)).Once();
  Verify(Method(ArduinoFake(), digitalWrite).Using(readEnablePin, LOW)).Once();
  Verify(Method(ArduinoFake(), digitalWrite).Using(writeEnablePin, LOW)).Once();
}

TEST_F(RS485BusTest, writing_a_byte_verifies_it_was_written) {
  buffer << 0x37;  // Next byte to be read
  When(Method(spy, available)).Return(0, 1);

  EXPECT_EQ(bus8.write(0x37), WriteStatus::OK);

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

TEST_F(RS485BusTest, writing_a_byte_when_a_different_one_is_read) {
  buffer << 0x21;  // Next byte to be read
  When(Method(spy, available)).Return(0, 1, 0);

  EXPECT_EQ(bus8.write(0x37), WriteStatus::FAILED_READ_BACK);

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

TEST_F(RS485BusTest, writing_a_byte_when_a_different_one_is_read_first) {
  buffer << 0x21 << 0x05 << 0x37;  // Next byte to be read
  When(Method(spy, available)).Return(0, 3, 2, 1, 0);

  EXPECT_EQ(bus8.write(0x37), WriteStatus::UNEXPECTED_EXTRA_BYTES);

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

  EXPECT_EQ(bus8[0], 0x21);
  EXPECT_EQ(bus8[1], 0x05);
  EXPECT_EQ(bus8[2], -1);
}

TEST_F(RS485BusTest, writing_a_byte_when_no_new_byte_is_available) {
  bus8.setReadBackRetries(0);
  bus8.setReadBackDelayMs(2);

  EXPECT_EQ(bus8.write(0x37), WriteStatus::NO_READ_TIMEOUT);

  Verify(
    Method(spy, available),
    Method(ArduinoFake(), digitalWrite).Using(writeEnablePin, HIGH),
    Method(spy, write).Using(0x37),
    Method(ArduinoFake(), digitalWrite).Using(writeEnablePin, LOW),
    Method(spy, available)
  ).Once();

  VerifyNoOtherInvocations(spy);
}

TEST_F(RS485BusTest, writing_a_byte_when_no_new_byte_is_available_with_delays) {
  bus8.setReadBackDelayMs(5);
  bus8.setReadBackRetries(3);

  EXPECT_EQ(bus8.write(0x37), WriteStatus::NO_READ_TIMEOUT);

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
TEST_F(RS485BusTest, writing_a_byte_that_eventually_returns_correct_value) {
  bus8.setReadBackDelayMs(7);
  bus8.setReadBackRetries(1);

  buffer << 0x21 << 0x37;
  When(Method(spy, available)).Return(0, 0, 1, 0, 1);

  EXPECT_EQ(bus8.write(0x37), WriteStatus::UNEXPECTED_EXTRA_BYTES);

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

  EXPECT_EQ(bus8[0], 0x21);
  EXPECT_EQ(bus8[1], -1);
}

TEST_F(RS485BusTest, writing_a_byte_if_that_byte_is_in_that_prefetched_buffer) {
  buffer << 0x21 << 0x37;
  bus8.fetch();
  spy.ClearInvocationHistory();

  EXPECT_EQ(bus8[0], 0x21);
  EXPECT_EQ(bus8[1], 0x37);
  EXPECT_EQ(bus8[2], -1);

  buffer << 0x37; // What we're about to write
  When(Method(spy, available)).Return(0, 1);

  EXPECT_EQ(bus8.write(0x37), WriteStatus::OK);

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

TEST_F(RS485BusTest, fetching_bytes_before_write_returns_no_write_new_bytes) {
  bus8.setPreFetchDelayMs(7);
  bus8.setPreFetchRetries(2);

  buffer << 0x21 << 0x34;
  When(Method(spy, available)).Return(1, 0, 0, 1, 0, 0, 0, 0);

  EXPECT_EQ(bus8.write(0x37), WriteStatus::NO_WRITE_NEW_BYTES);

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

  EXPECT_EQ(bus8[0], 0x21);
  EXPECT_EQ(bus8[1], 0x34);
  EXPECT_EQ(bus8[2], -1);
}

TEST_F(RS485BusTest, write_when_buffer_should_already_be_full) {
  buffer << 0x21 << 0x30 << 0x37;
  bus2.setReadBackRetries(0);

  EXPECT_EQ(bus2.write(0x37), WriteStatus::NO_WRITE_BUFFER_FULL);

  Verify(
    Method(spy, available),
    Method(spy, read),
    Method(spy, available),
    Method(spy, read)
  ).Once();

  Verify(Method(spy, write)).Never();
  VerifyNoOtherInvocations(spy);
  VerifyNoOtherInvocations(Method(ArduinoFake(), digitalWrite));

  EXPECT_EQ(bus2[0], 0x21);
  EXPECT_EQ(bus2[1], 0x30);
  EXPECT_EQ(bus2[2], -1);
  EXPECT_EQ(buffer.available(), 1); // 1 byte style available
}

/**
 * Simulate what would happen if no bytes appeared available, so we wrote our byte, but then
 * proceeded to see other new bytes that filled the buffer.
 */
TEST_F(RS485BusTest, write_when_buffer_becomes_full) {
  buffer << 0x21 << 0x30 << 0x37;
  bus2.setReadBackRetries(0);

  When(Method(spy, available)).Return(0, 3, 2, 1);

  EXPECT_EQ(bus2.write(0x37), WriteStatus::READ_BUFFER_FULL);

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

  EXPECT_EQ(bus2[0], 0x21);
  EXPECT_EQ(bus2[1], 0x30);
  EXPECT_EQ(bus2[2], -1);
  EXPECT_EQ(buffer.available(), 1); // 1 byte style available
}

TEST_F(RS485BusTest, write_when_buffer_becomes_full_but_no_more_is_available) {
  buffer << 0x21 << 0x30;
  bus2.setReadBackRetries(0);

  When(Method(spy, available)).Return(0, 2, 1, 0);

  EXPECT_EQ(bus2.write(0x37), WriteStatus::FAILED_READ_BACK);

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

  EXPECT_EQ(bus2[0], 0x21);
  EXPECT_EQ(bus2[1], 0x30);
  EXPECT_EQ(bus2[2], -1);
}

TEST_F(RS485BusTest, read_calls_fetch) {
  buffer << 1 << 2 << 3;

  // We haven't called fetch internally
  EXPECT_EQ(bus8.available(), 0);

  EXPECT_EQ(bus8.read(), 1);
  EXPECT_EQ(bus8.available(), 2);

  EXPECT_EQ(bus8.read(), 2);
  EXPECT_EQ(bus8.available(), 1);

  EXPECT_EQ(bus8.read(), 3);
  EXPECT_EQ(bus8.available(), 0);

  EXPECT_EQ(bus8.read(), -1);
  EXPECT_EQ(bus8.available(), 0);
}

TEST_F(RS485BusTest, read_fetches_even_if_not_all_bytes_have_been_read) {
  buffer << 1;

  bus8.fetch();
  buffer << 2 << 3;
  EXPECT_EQ(bus8.available(), 1);

  EXPECT_EQ(bus8.read(), 1);
  EXPECT_EQ(bus8.available(), 2);

  EXPECT_EQ(bus8.read(), 2);
  EXPECT_EQ(bus8.available(), 1);

  EXPECT_EQ(bus8.read(), 3);
  EXPECT_EQ(bus8.available(), 0);

  EXPECT_EQ(bus8.read(), -1);
  EXPECT_EQ(bus8.available(), 0);
}

TEST_F(RS485BusTest, read_clears_full_flag) {
  buffer << 1 << 2;

  bus2.fetch();
  EXPECT_EQ(bus2.available(), 2);
  EXPECT_TRUE(bus2.isBufferFull());

  EXPECT_EQ(bus2.read(), 1);
  EXPECT_EQ(bus2.available(), 1);
  EXPECT_FALSE(bus2.isBufferFull());

  EXPECT_EQ(bus2.read(), 2);
  EXPECT_EQ(bus2.available(), 0);
  EXPECT_FALSE(bus2.isBufferFull());

  EXPECT_EQ(bus2.read(), -1);
  EXPECT_EQ(bus2.available(), 0);
  EXPECT_FALSE(bus2.isBufferFull());
}
