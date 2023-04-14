#pragma once

#include "../assertable_bus_io.hpp"
#include "../fixtures.h"

#include <ArduinoFake.h>

#include "rs485/rs485bus.hpp"

using namespace fakeit;

class RS485BusTest : public PrepBus {
public:
  RS485BusTest(): PrepBus(),
    bus2(busIO, readEnablePin, writeEnablePin),
    bus8(busIO, readEnablePin, writeEnablePin) {}

  void SetUp() {
    ArduinoFake().ClearInvocationHistory();

    bus2.setSettleTime(settleTime);
    bus8.setSettleTime(settleTime);
  };

  const TimeMilliseconds_t settleTime = 9;
  AssertableBusIO busIO;
  Mock<AssertableBusIO> spy = busIO.spy();

  RS485Bus<2> bus2;
  RS485Bus<8> bus8;
};

TEST_F(RS485BusTest, buffer_size_is_template_parameter) {
  EXPECT_EQ(2, bus2.bufferSize());
  EXPECT_EQ(8, bus8.bufferSize());
}

TEST_F(RS485BusTest, by_default_available_returns_0) {
  EXPECT_EQ(0, bus8.available());
}

TEST_F(RS485BusTest, empty_buffer_does_nothing) {
  bus8.fetch();

  EXPECT_EQ(0, bus8.available());
  EXPECT_EQ(0, busIO.available());
}

TEST_F(RS485BusTest, can_fetch_available_bytes) {
  busIO << 1 << 2 << 3;

  EXPECT_EQ(0, bus8.available());

  EXPECT_EQ(3, bus8.fetch());
  EXPECT_EQ(3, bus8.available());
  EXPECT_EQ(0, busIO.available());
  EXPECT_EQ(1, bus8[0]);
  EXPECT_EQ(2, bus8[1]);
  EXPECT_EQ(3, bus8[2]);
  EXPECT_EQ(-1, bus8[3]);

  // Read another byte in to the read/write busIO, but don't fetch it
  busIO << 4;
  EXPECT_EQ(3, bus8.available());
  EXPECT_EQ(1, busIO.available());
  EXPECT_EQ(1, bus8[0]);
  EXPECT_EQ(2, bus8[1]);
  EXPECT_EQ(3, bus8[2]);
  EXPECT_EQ(-1, bus8[3]);

  EXPECT_EQ(1, bus8.fetch());
  EXPECT_EQ(1, bus8[0]);
  EXPECT_EQ(2, bus8[1]);
  EXPECT_EQ(3, bus8[2]);
  EXPECT_EQ(4, bus8[3]);
  EXPECT_EQ(-1, bus8[4]);
}

TEST_F(RS485BusTest, will_not_fetch_bytes_if_internal_buffer_is_full) {
  busIO << 1 << 2 << 3;

  EXPECT_EQ(3, busIO.available());
  EXPECT_EQ(0, bus2.available());

  bus2.fetch();

  EXPECT_EQ(1, busIO.available());
  EXPECT_EQ(2, bus2.available());

  EXPECT_EQ(1, bus2[0]);
  EXPECT_EQ(2, bus2[1]);
  EXPECT_EQ(-1, bus2[2]);

  EXPECT_EQ(3, busIO[0]);
  EXPECT_EQ(-1, busIO[1]);
}

TEST_F(RS485BusTest, by_default_read_pin_is_enabled_and_write_is_disabled) {
  RS485Bus<8> bus(busIO, readEnablePin, writeEnablePin);

  Verify(Method(ArduinoFake(), pinMode).Using(readEnablePin, OUTPUT)).Once();
  Verify(Method(ArduinoFake(), pinMode).Using(writeEnablePin, OUTPUT)).Once();
  Verify(Method(ArduinoFake(), digitalWrite).Using(readEnablePin, LOW)).Once();
  Verify(Method(ArduinoFake(), digitalWrite).Using(writeEnablePin, LOW)).Once();
}

TEST_F(RS485BusTest, writing_a_byte_verifies_it_was_written) {
  busIO << 0x37;  // Next byte to be read
  When(Method(spy, available)).Return(0, 1);

  EXPECT_EQ(WriteResult::OK, bus8.write(0x37));

  Verify(
    Method(spy, available),
    Method(ArduinoFake(), digitalWrite).Using(writeEnablePin, HIGH),
    Method(ArduinoFake(), delayMicroseconds).Using(settleTime),
    Method(spy, write).Using(0x37),
    Method(ArduinoFake(), delayMicroseconds).Using(settleTime),
    Method(ArduinoFake(), digitalWrite).Using(writeEnablePin, LOW),
    Method(ArduinoFake(), delayMicroseconds).Using(settleTime),
    Method(spy, available),
    Method(spy, read)
  ).Once();

  VerifyNoOtherInvocations(spy);
}

TEST_F(RS485BusTest, writing_a_byte_with_writing_already_enabled) {
  busIO << 0x37 << 0x46;  // Next byte to be read

  bus8.enableWrite(true);

  ArduinoFake().ClearInvocationHistory();

  When(Method(spy, available)).Return(0, 1);
  EXPECT_EQ(WriteResult::OK, bus8.write(0x37));

  Verify(
    Method(spy, available),
    Method(spy, write).Using(0x37),
    Method(ArduinoFake(), delayMicroseconds).Using(settleTime),
    Method(spy, available),
    Method(spy, read)
  ).Once();

  VerifyNoOtherInvocations(spy);

  When(Method(spy, available)).Return(0, 1);
  EXPECT_EQ(WriteResult::OK, bus8.write(0x46));

  Verify(
    Method(spy, available),
    Method(spy, write).Using(0x37),
    Method(ArduinoFake(), delayMicroseconds).Using(settleTime),
    Method(spy, available),
    Method(spy, read)
  ).Once();
}

TEST_F(RS485BusTest, writing_a_byte_with_a_different_settle_time) {
  bus8.setSettleTime(6);
  busIO << 0x37;  // Next byte to be read
  When(Method(spy, available)).Return(0, 1);

  EXPECT_EQ(WriteResult::OK, bus8.write(0x37));

  Verify(
    Method(spy, available),
    Method(ArduinoFake(), digitalWrite).Using(writeEnablePin, HIGH),
    Method(ArduinoFake(), delayMicroseconds).Using(6),
    Method(spy, write).Using(0x37),
    Method(ArduinoFake(), delayMicroseconds).Using(6),
    Method(ArduinoFake(), digitalWrite).Using(writeEnablePin, LOW),
    Method(ArduinoFake(), delayMicroseconds).Using(6),
    Method(spy, available),
    Method(spy, read)
  ).Once();

  VerifyNoOtherInvocations(spy);
}

TEST_F(RS485BusTest, writing_a_byte_when_a_different_one_is_read) {
  busIO << 0x21;  // Next byte to be read
  When(Method(spy, available)).Return(0, 1, 0);

  EXPECT_EQ(WriteResult::FAILED_READ_BACK, bus8.write(0x37));

  Verify(
    Method(spy, available),
    Method(ArduinoFake(), digitalWrite).Using(writeEnablePin, HIGH),
    Method(ArduinoFake(), delayMicroseconds).Using(settleTime),
    Method(spy, write).Using(0x37),
    Method(ArduinoFake(), delayMicroseconds).Using(settleTime),
    Method(ArduinoFake(), digitalWrite).Using(writeEnablePin, LOW),
    Method(ArduinoFake(), delayMicroseconds).Using(settleTime),
    Method(spy, available),
    Method(spy, read),
    Method(spy, available)
  ).Once();

  VerifyNoOtherInvocations(spy);
}

TEST_F(RS485BusTest, writing_a_byte_when_a_different_one_is_read_first) {
  busIO << 0x21 << 0x05 << 0x37;  // Next byte to be read
  When(Method(spy, available)).Return(0, 3, 2, 1, 0);

  EXPECT_EQ(WriteResult::UNEXPECTED_EXTRA_BYTES, bus8.write(0x37));

  Verify(
    Method(spy, available), // Returns 0
    Method(ArduinoFake(), digitalWrite).Using(writeEnablePin, HIGH),
    Method(ArduinoFake(), delayMicroseconds).Using(settleTime),
    Method(spy, write).Using(0x37),
    Method(ArduinoFake(), delayMicroseconds).Using(settleTime),
    Method(ArduinoFake(), digitalWrite).Using(writeEnablePin, LOW),
    Method(ArduinoFake(), delayMicroseconds).Using(settleTime),
    Method(spy, available), // Returns 3
    Method(spy, read),
    Method(spy, available), // Returns 2
    Method(spy, read),
    Method(spy, available), // Returns 1
    Method(spy, read)
  ).Once();

  VerifyNoOtherInvocations(spy);

  EXPECT_EQ(0x21, bus8[0]);
  EXPECT_EQ(0x05, bus8[1]);
  EXPECT_EQ(-1, bus8[2]);
}

TEST_F(RS485BusTest, writing_a_byte_when_no_new_byte_is_available) {
  bus8.setReadBackRetries(0);
  bus8.setReadBackDelay(2);

  EXPECT_EQ(WriteResult::NO_READ_TIMEOUT, bus8.write(0x37));

  Verify(
    Method(spy, available),
    Method(ArduinoFake(), digitalWrite).Using(writeEnablePin, HIGH),
    Method(ArduinoFake(), delayMicroseconds).Using(settleTime),
    Method(spy, write).Using(0x37),
    Method(ArduinoFake(), delayMicroseconds).Using(settleTime),
    Method(ArduinoFake(), digitalWrite).Using(writeEnablePin, LOW),
    Method(ArduinoFake(), delayMicroseconds).Using(settleTime),
    Method(spy, available)
  ).Once();

  VerifyNoOtherInvocations(spy);
}

TEST_F(RS485BusTest, writing_a_byte_when_no_new_byte_is_available_with_delays) {
  bus8.setReadBackDelay(5);
  bus8.setReadBackRetries(3);

  EXPECT_EQ(WriteResult::NO_READ_TIMEOUT, bus8.write(0x37));

  Verify(
    Method(spy, available),
    Method(ArduinoFake(), digitalWrite).Using(writeEnablePin, HIGH),
    Method(ArduinoFake(), delayMicroseconds).Using(settleTime),
    Method(spy, write).Using(0x37),
    Method(ArduinoFake(), delayMicroseconds).Using(settleTime),
    Method(ArduinoFake(), digitalWrite).Using(writeEnablePin, LOW),
    Method(ArduinoFake(), delayMicroseconds).Using(settleTime),
    Method(spy, available),
    Method(ArduinoFake(), delayMicroseconds).Using(5),
    Method(spy, available),
    Method(ArduinoFake(), delayMicroseconds).Using(5),
    Method(spy, available),
    Method(ArduinoFake(), delayMicroseconds).Using(5),
    Method(spy, available)
  ).Once();

  VerifyNoOtherInvocations(spy);
}

/**
 * This method is kinda weird because we also test that the retry counter gets reset when
 * a byte is read, even if that byte is incorrect.
 */
TEST_F(RS485BusTest, writing_a_byte_that_eventually_returns_correct_value) {
  bus8.setReadBackDelay(7);
  bus8.setReadBackRetries(1);

  busIO << 0x21 << 0x37;
  When(Method(spy, available)).Return(0, 0, 1, 0, 1);

  EXPECT_EQ(WriteResult::UNEXPECTED_EXTRA_BYTES, bus8.write(0x37));

  Verify(
    Method(spy, available), // Returns 0
    Method(ArduinoFake(), digitalWrite).Using(writeEnablePin, HIGH),
    Method(ArduinoFake(), delayMicroseconds).Using(settleTime),
    Method(spy, write).Using(0x37),
    Method(ArduinoFake(), delayMicroseconds).Using(settleTime),
    Method(ArduinoFake(), digitalWrite).Using(writeEnablePin, LOW),
    Method(ArduinoFake(), delayMicroseconds).Using(settleTime),
    Method(spy, available), // Returns 0
    Method(ArduinoFake(), delayMicroseconds).Using(7),
    Method(spy, available), // Returns 1
    Method(spy, read),
    Method(spy, available), // Returns 0
    Method(ArduinoFake(), delayMicroseconds).Using(7),
    Method(spy, available), // Returns 1
    Method(spy, read)
  ).Once();

  VerifyNoOtherInvocations(spy);

  EXPECT_EQ(0x21, bus8[0]);
  EXPECT_EQ(-1, bus8[1]);
}

TEST_F(RS485BusTest, writing_a_byte_if_that_byte_is_in_that_prefetched_buffer) {
  busIO << 0x21 << 0x37;
  bus8.fetch();
  spy.ClearInvocationHistory();

  EXPECT_EQ(0x21, bus8[0]);
  EXPECT_EQ(0x37, bus8[1]);
  EXPECT_EQ(-1, bus8[2]);

  busIO << 0x37; // What we're about to write
  When(Method(spy, available)).Return(0, 1);

  EXPECT_EQ(WriteResult::OK, bus8.write(0x37));

  Verify(
    Method(spy, available),  // Returns 0
    Method(ArduinoFake(), digitalWrite).Using(writeEnablePin, HIGH),
    Method(ArduinoFake(), delayMicroseconds).Using(settleTime),
    Method(spy, write).Using(0x37),
    Method(ArduinoFake(), delayMicroseconds).Using(settleTime),
    Method(ArduinoFake(), digitalWrite).Using(writeEnablePin, LOW),
    Method(ArduinoFake(), delayMicroseconds).Using(settleTime),
    Method(spy, available),  // Returns 1
    Method(spy, read)
  ).Once();

  VerifyNoOtherInvocations(spy);
}

TEST_F(RS485BusTest, fetching_bytes_before_write_returns_no_write_new_bytes) {
  bus8.setPreFetchDelay(7);
  bus8.setPreFetchRetries(2);

  busIO << 0x21 << 0x34;
  When(Method(spy, available)).Return(1, 0, 0, 1, 0, 0, 0, 0);

  EXPECT_EQ(WriteResult::NO_WRITE_NEW_BYTES, bus8.write(0x37));

  Verify(
    Method(spy, available), // Returns 1, fetch #1
    Method(spy, read),      // 0x21
    Method(spy, available), // Returns 0, fetch #1
    Method(ArduinoFake(), delayMicroseconds).Using(7),
    Method(spy, available), // Returns 0, fetch #2
    Method(ArduinoFake(), delayMicroseconds).Using(7),
    Method(spy, available), // Returns 1, fetch #3
    Method(spy, read),      // 0x34
    Method(spy, available), // Returns 0, fetch #3
    Method(ArduinoFake(), delayMicroseconds).Using(7),
    Method(spy, available), // Returns 0, fetch #4
    Method(ArduinoFake(), delayMicroseconds).Using(7),
    Method(spy, available), // Returns 0, fetch #5
    Method(ArduinoFake(), delayMicroseconds).Using(7),
    Method(spy, available)  // Returns 0, fetch #6
  ).Once();

  Verify(Method(spy, write)).Never();
  VerifyNoOtherInvocations(spy);
  VerifyNoOtherInvocations(Method(ArduinoFake(), digitalWrite));

  EXPECT_EQ(0x21, bus8[0]);
  EXPECT_EQ(0x34, bus8[1]);
  EXPECT_EQ(-1, bus8[2]);
}

TEST_F(RS485BusTest, write_when_buffer_should_already_be_full) {
  busIO << 0x21 << 0x30 << 0x37;
  bus2.setReadBackRetries(0);

  EXPECT_EQ(WriteResult::NO_WRITE_BUFFER_FULL, bus2.write(0x37));

  Verify(
    Method(spy, available),
    Method(spy, read),
    Method(spy, available),
    Method(spy, read)
  ).Once();

  Verify(Method(spy, write)).Never();
  VerifyNoOtherInvocations(spy);
  VerifyNoOtherInvocations(Method(ArduinoFake(), digitalWrite));

  EXPECT_EQ(0x21, bus2[0]);
  EXPECT_EQ(0x30, bus2[1]);
  EXPECT_EQ(-1, bus2[2]);
  EXPECT_EQ(1, busIO.available()); // 1 byte style available
}

/**
 * Simulate what would happen if no bytes appeared available, so we wrote our byte, but then
 * proceeded to see other new bytes that filled the busIO.
 */
TEST_F(RS485BusTest, write_when_buffer_becomes_full) {
  busIO << 0x21 << 0x30 << 0x37;
  bus2.setReadBackRetries(0);

  When(Method(spy, available)).Return(0, 3, 2, 1);

  EXPECT_EQ(WriteResult::READ_BUFFER_FULL, bus2.write(0x37));

  Verify(
    Method(spy, available), // Returns 0
    Method(ArduinoFake(), digitalWrite).Using(writeEnablePin, HIGH),
    Method(ArduinoFake(), delayMicroseconds).Using(settleTime),
    Method(spy, write).Using(0x37),
    Method(ArduinoFake(), delayMicroseconds).Using(settleTime),
    Method(ArduinoFake(), digitalWrite).Using(writeEnablePin, LOW),
    Method(ArduinoFake(), delayMicroseconds).Using(settleTime),
    Method(spy, available), // Returns 3
    Method(spy, read),
    Method(spy, available), // Returns 2
    Method(spy, read),
    Method(spy, available) // Returns 1
  ).Once();

  VerifyNoOtherInvocations(spy);

  EXPECT_EQ(0x21, bus2[0]);
  EXPECT_EQ(0x30, bus2[1]);
  EXPECT_EQ(-1, bus2[2]);
  EXPECT_EQ(1, busIO.available()); // 1 byte still available
}

TEST_F(RS485BusTest, write_when_buffer_becomes_full_but_no_more_is_available) {
  busIO << 0x21 << 0x30;
  bus2.setReadBackRetries(0);

  When(Method(spy, available)).Return(0, 2, 1, 0);

  EXPECT_EQ(WriteResult::FAILED_READ_BACK, bus2.write(0x37));

  Verify(
    Method(spy, available), // Returns 0
    Method(ArduinoFake(), digitalWrite).Using(writeEnablePin, HIGH),
    Method(ArduinoFake(), delayMicroseconds).Using(settleTime),
    Method(spy, write).Using(0x37),
    Method(ArduinoFake(), delayMicroseconds).Using(settleTime),
    Method(ArduinoFake(), digitalWrite).Using(writeEnablePin, LOW),
    Method(ArduinoFake(), delayMicroseconds).Using(settleTime),
    Method(spy, available), // Returns 2
    Method(spy, read),
    Method(spy, available), // Returns 1
    Method(spy, read),
    Method(spy, available) // Returns 0
  ).Once();

  VerifyNoOtherInvocations(spy);

  EXPECT_EQ(0x21, bus2[0]);
  EXPECT_EQ(0x30, bus2[1]);
  EXPECT_EQ(-1, bus2[2]);
}

TEST_F(RS485BusTest, read_calls_fetch) {
  busIO << 1 << 2 << 3;

  // We haven't called fetch internally
  EXPECT_EQ(0, bus8.available());

  EXPECT_EQ(1, bus8.read());
  EXPECT_EQ(2, bus8.available());

  EXPECT_EQ(2, bus8.read());
  EXPECT_EQ(1, bus8.available());

  EXPECT_EQ(3, bus8.read());
  EXPECT_EQ(0, bus8.available());

  EXPECT_EQ(-1, bus8.read());
  EXPECT_EQ(0, bus8.available());
}

TEST_F(RS485BusTest, read_fetches_even_if_not_all_bytes_have_been_read) {
  busIO << 1;

  bus8.fetch();
  busIO << 2 << 3;
  EXPECT_EQ(1, bus8.available());

  EXPECT_EQ(1, bus8.read());
  EXPECT_EQ(2, bus8.available());

  EXPECT_EQ(2, bus8.read());
  EXPECT_EQ(1, bus8.available());

  EXPECT_EQ(3, bus8.read());
  EXPECT_EQ(0, bus8.available());

  EXPECT_EQ(-1, bus8.read());
  EXPECT_EQ(0, bus8.available());
}

TEST_F(RS485BusTest, read_clears_full_flag) {
  busIO << 1 << 2;

  bus2.fetch();
  EXPECT_EQ(2, bus2.available());
  EXPECT_TRUE(bus2.isBufferFull());

  EXPECT_EQ(1, bus2.read());
  EXPECT_EQ(1, bus2.available());
  EXPECT_FALSE(bus2.isBufferFull());

  EXPECT_EQ(2, bus2.read());
  EXPECT_EQ(0, bus2.available());
  EXPECT_FALSE(bus2.isBufferFull());

  EXPECT_EQ(-1, bus2.read());
  EXPECT_EQ(0, bus2.available());
  EXPECT_FALSE(bus2.isBufferFull());
}
