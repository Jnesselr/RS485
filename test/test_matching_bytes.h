#ifdef UNIT_TEST
#include <unity.h>

#include "assertable_buffer.h"
#include "rs485bus.hpp"
#include "matching_bytes.h"
#include <ArduinoFake.h>

namespace PacketMatchingBytesTest {
  const uint8_t readEnablePin = 13;
  const uint8_t writeEnablePin = 14;

  void setUpArduinoFake() {
    When(Method(ArduinoFake(), pinMode)).AlwaysReturn();
    When(Method(ArduinoFake(), digitalWrite)).AlwaysReturn();
  }

  void test_single_byte_odd() {
    AssertableBuffer buffer;

    setUpArduinoFake();
    RS485Bus<8> bus(buffer, readEnablePin, writeEnablePin);
    buffer << 0x01;
    bus.fetch();

    PacketMatchingBytes packetInfo;

    int endIndex = 0;
    TEST_ASSERT_EQUAL_INT(PacketStatus::NO, packetInfo.isPacket(bus, 0, endIndex));
  }

  void test_single_byte_even() {
    AssertableBuffer buffer;

    setUpArduinoFake();
    RS485Bus<8> bus(buffer, readEnablePin, writeEnablePin);
    buffer << 0x02;
    bus.fetch();

    PacketMatchingBytes packetInfo;

    int endIndex = 0;
    TEST_ASSERT_EQUAL_INT(PacketStatus::NOT_ENOUGH_BYTES, packetInfo.isPacket(bus, 0, endIndex));
  }
  
  void test_various_packets() {
    AssertableBuffer buffer;

    setUpArduinoFake();
    RS485Bus<8> bus(buffer, readEnablePin, writeEnablePin);
    buffer << 0x01 << 0x02 << 0x03 << 0x01 << 0xff << 0xfe << 0x02;
    bus.fetch();

    PacketMatchingBytes packetInfo;

    int endIndex = 6;
    TEST_ASSERT_EQUAL_INT(PacketStatus::YES, packetInfo.isPacket(bus, 0, endIndex));
    TEST_ASSERT_EQUAL_INT(3, endIndex);

    endIndex = 6;
    TEST_ASSERT_EQUAL_INT(PacketStatus::YES, packetInfo.isPacket(bus, 1, endIndex));
    TEST_ASSERT_EQUAL_INT(6, endIndex);

    TEST_ASSERT_EQUAL_INT(PacketStatus::NO, packetInfo.isPacket(bus, 2, endIndex));
    TEST_ASSERT_EQUAL_INT(6, endIndex);

    TEST_ASSERT_EQUAL_INT(PacketStatus::NO, packetInfo.isPacket(bus, 3, endIndex));
    TEST_ASSERT_EQUAL_INT(6, endIndex);

    TEST_ASSERT_EQUAL_INT(PacketStatus::NO, packetInfo.isPacket(bus, 4, endIndex));
    TEST_ASSERT_EQUAL_INT(6, endIndex);

    TEST_ASSERT_EQUAL_INT(PacketStatus::NOT_ENOUGH_BYTES, packetInfo.isPacket(bus, 5, endIndex));
    TEST_ASSERT_EQUAL_INT(6, endIndex);

    TEST_ASSERT_EQUAL_INT(PacketStatus::NOT_ENOUGH_BYTES, packetInfo.isPacket(bus, 6, endIndex));
    TEST_ASSERT_EQUAL_INT(6, endIndex);
  }

  void run_tests() {
    RUN_TEST(test_single_byte_odd);
    RUN_TEST(test_single_byte_even);
    RUN_TEST(test_various_packets);
  }
}

#endif