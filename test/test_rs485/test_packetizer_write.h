#pragma once

#include "../assertable_buffer.h"
#include "../matching_bytes.h"
#include "rs485bus_base.h"
#include "packetizer.h"
#include "fakeit/fakeit.hpp"

#include <gtest/gtest.h>

using namespace fakeit;

class PacketizerWriteTest : public ::testing::Test {
protected:
  PacketizerWriteTest() :
    packetizer(bus, packetInfo) {}

  void SetUp() {
    When(Method(ArduinoFake(), delay)).AlwaysReturn();
    When(Method(ArduinoFake(), millis)).AlwaysReturn(0);
    When(Method(fakeBus, bufferSize)).AlwaysReturn(8);
    packetizer.setMaxWriteTimeout(0);

    ArduinoFake().ClearInvocationHistory();
  };

  Mock<RS485BusBase> fakeBus;
  RS485BusBase& bus = fakeBus.get();
  PacketMatchingBytes packetInfo;  // Only needed for packetizer constructor
  Packetizer packetizer;

  // Buffer is 8 bytes, but we'll only write 7
  unsigned char buffer[8] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0x00};

  PacketWriteStatus writePacket() {
    return packetizer.writePacket(buffer, 7);
  }
};

TEST_F(PacketizerWriteTest, write_package_with_no_interruptions) {
  When(Method(fakeBus, write)).AlwaysReturn(WriteStatus::OK);

  PacketWriteStatus status = this->writePacket();
  EXPECT_EQ(status, PacketWriteStatus::OK);

  Verify(
    Method(fakeBus, write).Using(0x12),
    Method(fakeBus, write).Using(0x34),
    Method(fakeBus, write).Using(0x56),
    Method(fakeBus, write).Using(0x78),
    Method(fakeBus, write).Using(0x9A),
    Method(fakeBus, write).Using(0xBC),
    Method(fakeBus, write).Using(0xDE)
  ).Once();

  VerifyNoOtherInvocations(Method(fakeBus, write));
}

TEST_F(PacketizerWriteTest, unexpected_extra_bytes_at_beginning_of_message) {
  When(Method(fakeBus, write)).AlwaysReturn(WriteStatus::OK);
  When(Method(fakeBus, write)(0x12)).AlwaysReturn(WriteStatus::UNEXPECTED_EXTRA_BYTES);

  PacketWriteStatus status = this->writePacket();
  EXPECT_EQ(status, PacketWriteStatus::OK);

  Verify(
    Method(fakeBus, write).Using(0x12),
    Method(fakeBus, write).Using(0x34),
    Method(fakeBus, write).Using(0x56),
    Method(fakeBus, write).Using(0x78),
    Method(fakeBus, write).Using(0x9A),
    Method(fakeBus, write).Using(0xBC),
    Method(fakeBus, write).Using(0xDE)
  ).Once();

  VerifyNoOtherInvocations(Method(fakeBus, write));
}

TEST_F(PacketizerWriteTest, unexpected_extra_bytes_in_middle_of_message) {
  When(Method(fakeBus, write)).AlwaysReturn(WriteStatus::OK);
  When(Method(fakeBus, write)(0x78)).AlwaysReturn(WriteStatus::UNEXPECTED_EXTRA_BYTES);

  PacketWriteStatus status = this->writePacket();
  EXPECT_EQ(status, PacketWriteStatus::FAILED_INTERRUPTED);

  Verify(
    Method(fakeBus, write).Using(0x12),
    Method(fakeBus, write).Using(0x34),
    Method(fakeBus, write).Using(0x56),
    Method(fakeBus, write).Using(0x78)
  ).Once();

  VerifyNoOtherInvocations(Method(fakeBus, write));
}

TEST_F(PacketizerWriteTest, read_buffer_full_at_beginning_of_message) {
  When(Method(fakeBus, write)).AlwaysReturn(WriteStatus::OK);
  When(Method(fakeBus, write)(0x12)).AlwaysReturn(WriteStatus::READ_BUFFER_FULL);

  PacketWriteStatus status = this->writePacket();
  EXPECT_EQ(status, PacketWriteStatus::FAILED_BUFFER_FULL);

  Verify(
    Method(fakeBus, write).Using(0x12)
  ).Once();

  VerifyNoOtherInvocations(Method(fakeBus, write));
}

TEST_F(PacketizerWriteTest, read_buffer_full_in_middle_of_message) {
  When(Method(fakeBus, write)).AlwaysReturn(WriteStatus::OK);
  When(Method(fakeBus, write)(0x78)).AlwaysReturn(WriteStatus::READ_BUFFER_FULL);

  PacketWriteStatus status = this->writePacket();
  EXPECT_EQ(status, PacketWriteStatus::FAILED_BUFFER_FULL);

  Verify(
    Method(fakeBus, write).Using(0x12),
    Method(fakeBus, write).Using(0x34),
    Method(fakeBus, write).Using(0x56),
    Method(fakeBus, write).Using(0x78)
  ).Once();

  VerifyNoOtherInvocations(Method(fakeBus, write));
}

TEST_F(PacketizerWriteTest, no_write_buffer_full_at_beginning_of_message) {
  When(Method(fakeBus, write)).AlwaysReturn(WriteStatus::OK);
  When(Method(fakeBus, write)(0x12)).AlwaysReturn(WriteStatus::NO_WRITE_BUFFER_FULL);

  PacketWriteStatus status = this->writePacket();
  EXPECT_EQ(status, PacketWriteStatus::FAILED_BUFFER_FULL);

  Verify(
    Method(fakeBus, write).Using(0x12)
  ).Once();

  VerifyNoOtherInvocations(Method(fakeBus, write));
}

TEST_F(PacketizerWriteTest, no_write_buffer_full_in_middle_of_message) {
  When(Method(fakeBus, write)).AlwaysReturn(WriteStatus::OK);
  When(Method(fakeBus, write)(0x78)).AlwaysReturn(WriteStatus::NO_WRITE_BUFFER_FULL);

  PacketWriteStatus status = this->writePacket();
  EXPECT_EQ(status, PacketWriteStatus::FAILED_BUFFER_FULL);

  Verify(
    Method(fakeBus, write).Using(0x12),
    Method(fakeBus, write).Using(0x34),
    Method(fakeBus, write).Using(0x56),
    Method(fakeBus, write).Using(0x78)
  ).Once();

  VerifyNoOtherInvocations(Method(fakeBus, write));
}

TEST_F(PacketizerWriteTest, no_read_timeout_at_beginning_of_message) {
  When(Method(fakeBus, write)).AlwaysReturn(WriteStatus::OK);
  When(Method(fakeBus, write)(0x12)).AlwaysReturn(WriteStatus::NO_READ_TIMEOUT);

  PacketWriteStatus status = this->writePacket();
  EXPECT_EQ(status, PacketWriteStatus::FAILED_INTERRUPTED);

  Verify(
    Method(fakeBus, write).Using(0x12)
  ).Once();

  VerifyNoOtherInvocations(Method(fakeBus, write));
}

TEST_F(PacketizerWriteTest, no_read_timeout_in_middle_of_message) {
  When(Method(fakeBus, write)).AlwaysReturn(WriteStatus::OK);
  When(Method(fakeBus, write)(0x78)).AlwaysReturn(WriteStatus::NO_READ_TIMEOUT);

  PacketWriteStatus status = this->writePacket();
  EXPECT_EQ(status, PacketWriteStatus::FAILED_INTERRUPTED);

  Verify(
    Method(fakeBus, write).Using(0x12),
    Method(fakeBus, write).Using(0x34),
    Method(fakeBus, write).Using(0x56),
    Method(fakeBus, write).Using(0x78)
  ).Once();

  VerifyNoOtherInvocations(Method(fakeBus, write));
}

TEST_F(PacketizerWriteTest, failed_read_back_at_beginning_of_message) {
  When(Method(fakeBus, write)).AlwaysReturn(WriteStatus::OK);
  When(Method(fakeBus, write)(0x12)).AlwaysReturn(WriteStatus::FAILED_READ_BACK);

  PacketWriteStatus status = this->writePacket();
  EXPECT_EQ(status, PacketWriteStatus::FAILED_INTERRUPTED);

  Verify(
    Method(fakeBus, write).Using(0x12)
  ).Once();

  VerifyNoOtherInvocations(Method(fakeBus, write));
}

TEST_F(PacketizerWriteTest, failed_read_back_in_middle_of_message) {
  When(Method(fakeBus, write)).AlwaysReturn(WriteStatus::OK);
  When(Method(fakeBus, write)(0x78)).AlwaysReturn(WriteStatus::FAILED_READ_BACK);

  PacketWriteStatus status = this->writePacket();
  EXPECT_EQ(status, PacketWriteStatus::FAILED_INTERRUPTED);

  Verify(
    Method(fakeBus, write).Using(0x12),
    Method(fakeBus, write).Using(0x34),
    Method(fakeBus, write).Using(0x56),
    Method(fakeBus, write).Using(0x78)
  ).Once();

  VerifyNoOtherInvocations(Method(fakeBus, write));
}

TEST_F(PacketizerWriteTest, no_write_new_bytes_at_beginning_of_message) {
  When(Method(fakeBus, write)).AlwaysReturn(WriteStatus::OK);
  When(Method(fakeBus, write)(0x12)).AlwaysReturn(WriteStatus::NO_WRITE_NEW_BYTES);

  PacketWriteStatus status = this->writePacket();
  EXPECT_EQ(status, PacketWriteStatus::FAILED_INTERRUPTED);

  Verify(
    Method(fakeBus, write).Using(0x12)
  ).Once();

  VerifyNoOtherInvocations(Method(fakeBus, write));
}

TEST_F(PacketizerWriteTest, no_write_new_bytes_in_middle_of_message) {
  When(Method(fakeBus, write)).AlwaysReturn(WriteStatus::OK);
  When(Method(fakeBus, write)(0x78)).AlwaysReturn(WriteStatus::NO_WRITE_NEW_BYTES);

  PacketWriteStatus status = this->writePacket();
  EXPECT_EQ(status, PacketWriteStatus::FAILED_INTERRUPTED);

  Verify(
    Method(fakeBus, write).Using(0x12),
    Method(fakeBus, write).Using(0x34),
    Method(fakeBus, write).Using(0x56),
    Method(fakeBus, write).Using(0x78)
  ).Once();

  VerifyNoOtherInvocations(Method(fakeBus, write));
}

TEST_F(PacketizerWriteTest, write_package_delays_to_ensure_quiet_time) {
  packetizer.setMaxWriteTimeout(100);
  packetizer.setBusQuietTime(100);
  When(Method(ArduinoFake(), millis)).AlwaysReturn(15);
  When(Method(fakeBus, fetch)).AlwaysReturn(0);

  When(Method(fakeBus, write)).AlwaysReturn(WriteStatus::OK);

  PacketWriteStatus status = this->writePacket();
  EXPECT_EQ(status, PacketWriteStatus::OK);

  Verify(
    Method(ArduinoFake(), millis),
    Method(ArduinoFake(), delay).Using(85),  // quiet time is 100 ms - 15 ms that millis() returns
    Method(fakeBus, fetch),
    Method(fakeBus, write).Using(0x12),
    Method(fakeBus, write).Using(0x34),
    Method(fakeBus, write).Using(0x56),
    Method(fakeBus, write).Using(0x78),
    Method(fakeBus, write).Using(0x9A),
    Method(fakeBus, write).Using(0xBC),
    Method(fakeBus, write).Using(0xDE)
  ).Once();

  VerifyNoOtherInvocations(Method(fakeBus, write));
}

TEST_F(PacketizerWriteTest, write_package_delays_to_ensure_quiet_time_in_noisy_environment) {
  packetizer.setMaxWriteTimeout(1000);
  packetizer.setBusQuietTime(100);

  // These two have to work together. millis then fetch then millis again to update last read time.
  // However, since we already know we've hit bytes in this function, we don't ask the current time
  // again, only delay until our next fetch.
  When(Method(ArduinoFake(), millis)).Return(15, 125, 230, 332);
  When(Method(fakeBus, fetch)).Return(1, 1, 1, 0);

  When(Method(fakeBus, write)).AlwaysReturn(WriteStatus::OK);

  PacketWriteStatus status = this->writePacket();
  EXPECT_EQ(status, PacketWriteStatus::OK);

  Verify(
    Method(ArduinoFake(), millis),  // Returns 15
    Method(ArduinoFake(), delay).Using(85),  // quiet time is 100 ms - 15 ms that millis() returns
    Method(fakeBus, fetch),  // Returns 1
    Method(ArduinoFake(), millis),  // Returns 125
    Method(ArduinoFake(), delay).Using(100),  // Just assume we're delaying the full 100.
    Method(fakeBus, fetch),  // Returns 1
    Method(ArduinoFake(), millis),  // Returns 230
    Method(ArduinoFake(), delay).Using(100),  // Just assume we're delaying the full 100.
    Method(fakeBus, fetch),  // Returns 1
    Method(ArduinoFake(), millis),  // Returns 332
    Method(ArduinoFake(), delay).Using(100),  // Just assume we're delaying the full 100.
    Method(fakeBus, fetch),  // Returns 0, last byte read time isn't updated
    Method(fakeBus, write).Using(0x12),
    Method(fakeBus, write).Using(0x34),
    Method(fakeBus, write).Using(0x56),
    Method(fakeBus, write).Using(0x78),
    Method(fakeBus, write).Using(0x9A),
    Method(fakeBus, write).Using(0xBC),
    Method(fakeBus, write).Using(0xDE)
  ).Once();

  VerifyNoOtherInvocations(Method(fakeBus, write));
}

TEST_F(PacketizerWriteTest, write_package_delays_until_timeout) {
  packetizer.setMaxWriteTimeout(300);
  packetizer.setBusQuietTime(100);

  When(Method(ArduinoFake(), millis)).Return(0, 100, 200, 300);
  When(Method(fakeBus, fetch)).AlwaysReturn(1);

  When(Method(fakeBus, write)).AlwaysReturn(WriteStatus::OK);

  PacketWriteStatus status = this->writePacket();
  EXPECT_EQ(status, PacketWriteStatus::FAILED_TIMEOUT);

  Verify(
    Method(ArduinoFake(), millis),  // Returns 0
    Method(ArduinoFake(), delay).Using(100),
    Method(fakeBus, fetch),
    Method(ArduinoFake(), millis),  // Returns 100
    Method(ArduinoFake(), delay).Using(100),
    Method(fakeBus, fetch),
    Method(ArduinoFake(), millis),  // Returns 200
    Method(ArduinoFake(), delay).Using(100),
    Method(fakeBus, fetch),
    Method(ArduinoFake(), millis)  // Returns 300
  ).Once();

  VerifyNoOtherInvocations(Method(fakeBus, write));
}

TEST_F(PacketizerWriteTest, read_affects_last_read_byte_time) {
  packetizer.setMaxReadTimeout(0);
  packetizer.setMaxWriteTimeout(0);
  packetizer.setBusQuietTime(100);
  When(Method(fakeBus, fetch)).Return(1_Time(1), 100_Times(0)); // Our first fetch during read has to have data to force a delay on write
  When(Method(fakeBus, write)).AlwaysReturn(WriteStatus::OK);

  When(Method(ArduinoFake(), millis)).Return(
    0,    // Read start time
    15,   // Read time of fetch
    30,   // Read time end of hasPacket, to check, against timeout, since no byte was found
    45    // Write start time
    );

  When(Method(fakeBus, available)).AlwaysReturn(7);
  When(Method(fakeBus, operator[])).AlwaysDo([&](const unsigned int& index) -> const int { return buffer[index]; });

  EXPECT_FALSE(packetizer.hasPacket());
  PacketWriteStatus status = this->writePacket();
  EXPECT_EQ(status, PacketWriteStatus::OK);

  Verify(
    Method(ArduinoFake(), millis),  // Returns 0
    Method(fakeBus, fetch),         // Returns 1
    Method(ArduinoFake(), millis),  // Returns 15
    Method(ArduinoFake(), millis),  // Returns 30
    Method(ArduinoFake(), millis),  // Returns 45
    Method(ArduinoFake(), delay).Using(70),
    Method(fakeBus, fetch),         // Returns 0
    Method(fakeBus, write).Using(0x12),
    Method(fakeBus, write).Using(0x34),
    Method(fakeBus, write).Using(0x56),
    Method(fakeBus, write).Using(0x78),
    Method(fakeBus, write).Using(0x9A),
    Method(fakeBus, write).Using(0xBC),
    Method(fakeBus, write).Using(0xDE)
  ).Once();

  VerifyNoOtherInvocations(Method(fakeBus, write));
}