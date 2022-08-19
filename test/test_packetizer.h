#ifdef UNIT_TEST
#include <unity.h>

#include "assertable_buffer.h"
#include "matching_bytes.h"
#include "rs485bus.hpp"
#include "packetizer.h"
#include "fakeit/fakeit.hpp"

using namespace fakeit;

namespace PacketizerTest {
  const uint8_t readEnablePin = 13;
  const uint8_t writeEnablePin = 14;
  
  void setUpArduinoFake() {
    When(Method(ArduinoFake(), pinMode)).AlwaysReturn();
    When(Method(ArduinoFake(), digitalWrite)).AlwaysReturn();
  }

  Mock<PacketMatchingBytes> spyMatchingBytes(PacketMatchingBytes& packetInfo) {
    Mock<PacketMatchingBytes> mySpy(packetInfo);

    Spy(Method(mySpy, isPacket));

    return mySpy;
  }

  void test_by_default_no_packets_are_available() {
    AssertableBuffer buffer;
    setUpArduinoFake();
    RS485Bus<8> bus(buffer, readEnablePin, writeEnablePin);
    PacketMatchingBytes packetInfo;
    Packetizer packetizer(bus, packetInfo);

    TEST_ASSERT_FALSE(packetizer.hasPacket());
    TEST_ASSERT_EQUAL_INT(0, packetizer.packetLength());
  }

  void test_can_get_simple_packet() {
    AssertableBuffer buffer;
    setUpArduinoFake();
    RS485Bus<8> bus(buffer, readEnablePin, writeEnablePin);
    PacketMatchingBytes packetInfo;
    Packetizer packetizer(bus, packetInfo);

    buffer << 0x01 << 0x02 << 0x03 << 0x02 << 0x04;

    // Ensure the bus hasn't fetched a single byte from the serial bus yet, meaning the packetizer calls fetch
    TEST_ASSERT_EQUAL_INT(0, bus.available());
    TEST_ASSERT_EQUAL_INT(0, packetizer.packetLength());

    TEST_ASSERT_TRUE(packetizer.hasPacket());
    TEST_ASSERT_EQUAL_INT(3, packetizer.packetLength());

    TEST_ASSERT_EQUAL_INT(4, bus.available()); // The first byte isn't part of the packet
    TEST_ASSERT_EQUAL_INT(0x02, bus[0]);
    TEST_ASSERT_EQUAL_INT(0x03, bus[1]);
    TEST_ASSERT_EQUAL_INT(0x02, bus[2]);
    TEST_ASSERT_EQUAL_INT(0x04, bus[3]);
    TEST_ASSERT_EQUAL_INT(-1, bus[4]);

    packetizer.clearPacket();
    TEST_ASSERT_FALSE(packetizer.hasPacket());
    TEST_ASSERT_EQUAL_INT(0, packetizer.packetLength());
    TEST_ASSERT_EQUAL_INT(1, bus.available());
    TEST_ASSERT_EQUAL_INT(0x04, bus[0]);
    TEST_ASSERT_EQUAL_INT(-1, bus[1]);
  }

  void test_only_no_bytes_get_skipped() {
    AssertableBuffer buffer;
    setUpArduinoFake();
    RS485Bus<8> bus(buffer, readEnablePin, writeEnablePin);
    PacketMatchingBytes packetInfo;
    Packetizer packetizer(bus, packetInfo);

    buffer << 0x01 << 0x03 << 0x02 << 0x05;

    // Ensure the bus hasn't fetched a single byte from the serial bus yet, meaning the packetizer calls fetch
    TEST_ASSERT_EQUAL_INT(0, bus.available());
    TEST_ASSERT_EQUAL_INT(0, packetizer.packetLength());

    TEST_ASSERT_FALSE(packetizer.hasPacket());
    TEST_ASSERT_EQUAL_INT(0, packetizer.packetLength());

    TEST_ASSERT_EQUAL_INT(2, bus.available());
    TEST_ASSERT_EQUAL_INT(0x02, bus[0]);  // Returns "not enough bytes", so isn't discarded
    TEST_ASSERT_EQUAL_INT(0x05, bus[1]);  // Returns "no"
    TEST_ASSERT_EQUAL_INT(-1, bus[2]);
  }

  void test_not_enough_bytes_get_skipped_if_buffer_is_full() {
    AssertableBuffer buffer;
    setUpArduinoFake();
    RS485Bus<2> bus(buffer, readEnablePin, writeEnablePin);
    PacketMatchingBytes packetInfo;
    Packetizer packetizer(bus, packetInfo);

    buffer << 0x02 << 0x04;

    // Ensure the bus hasn't fetched a single byte from the serial bus yet, meaning the packetizer calls fetch
    TEST_ASSERT_EQUAL_INT(0, bus.available());
    TEST_ASSERT_EQUAL_INT(0, packetizer.packetLength());

    TEST_ASSERT_FALSE(packetizer.hasPacket());
    TEST_ASSERT_EQUAL_INT(0, packetizer.packetLength());

    TEST_ASSERT_EQUAL_INT(1, bus.available());
    TEST_ASSERT_EQUAL_INT(0x04, bus[0]);  // Returns "not enough bytes", but we might read in more bytes so it stays
    TEST_ASSERT_EQUAL_INT(-1, bus[1]);
  }

  /**
   * This test is pretty gnarly. There's a couple of reasons for that. The first is the "hack" below to workaround verifying captured values.
   * The second has to do with the flow for the packetizer. This more closely resembles what actual usage would look like and it found some
   * issues that splitting up the test cases did not.
   *
   * Because it's long and hard to read, the verification sections are broken up by comments that start with "--".
   */
  void test_no_bytes_do_not_get_tested_again() {
    constexpr size_t ulSize = sizeof(unsigned long);  // Needs to match the date type used for recheckBitmap in Packetizer
    constexpr int bufferSize = uSize + 5;  // unsigned long size + 3 bytes we add + 2 just to be safe

    AssertableBuffer buffer;
    setUpArduinoFake();
    RS485Bus<bufferSize> bus(buffer, readEnablePin, writeEnablePin);
    PacketMatchingBytes packetInfo;
    Mock<PacketMatchingBytes> spy = spyMatchingBytes(packetInfo);
    Packetizer packetizer(bus, packetInfo);

    // AWFUL HACK ALERT - related: https://github.com/eranpeer/FakeIt/issues/274
    // Essentially, we can't verify captured arguments. So we have an array to verify if we've called in using the correct parameters.
    // We also don't want to re-implement what PacketMatchingBytes does so we create a new instance that we defer to, since our main instance is being spied on.
    bool wasCalledAssert[bufferSize] = { false };
    int endIndexAssert[bufferSize] = { 0 };
    PacketMatchingBytes basePacketInfo;
    When(Method(spy, isPacket)).AlwaysDo([&](const RS485BusBase& bus, const int& startIndex, int& endIndex)->PacketStatus {
      wasCalledAssert[startIndex] = true;
      PacketStatus result = basePacketInfo.isPacket(bus, startIndex, endIndex);

      endIndexAssert[startIndex] = endIndex;
      return result;
    });

    for(int i = 0; i < ulSize; i++) {
      // Even values of i is "not enough bytes", odd is "no" and we want both. We can't just alternate or we'll get a valid packet, though. We also need it to start with "not enough bytes" so nothing gets shifted.
      buffer << i;
    }
    
    // Ensure the bus hasn't fetched a single byte from the serial bus yet, meaning the packetizer calls fetch
    TEST_ASSERT_EQUAL_INT(0, bus.available());
    TEST_ASSERT_EQUAL_INT(0, packetizer.packetLength());

    TEST_ASSERT_FALSE(packetizer.hasPacket());
    TEST_ASSERT_EQUAL_INT(0, packetizer.packetLength());
    TEST_ASSERT_EQUAL_INT(ulSize, bus.available());

    // -- We want to verify that "isPacket" got called on everything

    std::string message;
    for(int i = 0; i < ulSize; i++) {
      message = "Was not called for start index " + std::to_string(i);
      TEST_ASSERT_TRUE_MESSAGE(wasCalledAssert[i], message.c_str());

      message = "Bad end index for start index " + std::to_string(i);
      TEST_ASSERT_EQUAL_INT_MESSAGE(ulSize - 1, endIndexAssert[i], message.c_str());
    }

    // -- Now we want to call hasPacket again and verify that nothing gets called because no new bytes became available

    // Reset everthing
    memset(wasCalledAssert, false, sizeof(bool) * bufferSize);
    memset(endIndexAssert, 0, sizeof(int) * bufferSize);

    // Call our hasPacket method
    TEST_ASSERT_FALSE(packetizer.hasPacket());
    TEST_ASSERT_EQUAL_INT(0, packetizer.packetLength());
    TEST_ASSERT_EQUAL_INT(ulSize, bus.available());

    // And finally, verify
    for(int i = 0; i < ulSize; i++) {
      message = "Was called for start index " + std::to_string(i) + " when it should not have been";
      TEST_ASSERT_FALSE_MESSAGE(wasCalledAssert[i], message.c_str());

      message = "Bad end index for start index " + std::to_string(i);
      TEST_ASSERT_EQUAL_INT_MESSAGE(0, endIndexAssert[i], message.c_str());
    }

    // -- We want to complete a packet that was "not enough bytes" before. Since the size of a long will always be even, we match with size - 2. (size - 1) is currently the last byte on the buffer.

    // Queue up an even number, plus a "no", plus a "not enough bytes"
    int validPacketByte = ulSize - 2;
    buffer << validPacketByte << (ulSize + 1) << (ulSize + 2);

    // Reset everthing
    memset(wasCalledAssert, false, sizeof(bool) * bufferSize);
    memset(endIndexAssert, 0, sizeof(int) * bufferSize);

    // Call our hasPacket method
    TEST_ASSERT_TRUE(packetizer.hasPacket());
    TEST_ASSERT_EQUAL_INT(3, packetizer.packetLength());
    TEST_ASSERT_EQUAL_INT(5, bus.available());  // Everything else got cleared out but the packet and our two extra bytes

    // And finally, verify
    for(int i = 0; i < ulSize; i++) {
      if(i % 2 == 0) {
        message = "Was not called for start index " + std::to_string(i);
        TEST_ASSERT_TRUE_MESSAGE(wasCalledAssert[i], message.c_str());

        if(i == validPacketByte) {
          message = "Bad end index for start index " + std::to_string(i);
          TEST_ASSERT_EQUAL_INT_MESSAGE(ulSize, endIndexAssert[i], message.c_str());  // Our valid packet byte adjusts the end index
        } else {
          message = "Bad end index for start index " + std::to_string(i);
          TEST_ASSERT_EQUAL_INT_MESSAGE(ulSize + 2, endIndexAssert[i], message.c_str());
        }
      } else {
        message = "Was called for start index " + std::to_string(i) + " when it should not have been";
        TEST_ASSERT_FALSE_MESSAGE(wasCalledAssert[i], message.c_str());

        message = "Bad end index for start index " + std::to_string(i);
        TEST_ASSERT_EQUAL_INT_MESSAGE(0, endIndexAssert[i], message.c_str());
      }
    }

    TEST_ASSERT_FALSE_MESSAGE(wasCalledAssert[ulSize], "No bytes after valid packet should have been tested");
    TEST_ASSERT_FALSE_MESSAGE(wasCalledAssert[ulSize+1], "No bytes after valid packet should have been tested");
    TEST_ASSERT_FALSE_MESSAGE(wasCalledAssert[ulSize+2], "No bytes after valid packet should have been tested");

    // -- Clearing the packet will result in (ulSize + 1) and (ulSize + 2) being on the bus. The first is "no" so goes away. We do this step to make sure previous times where things were not re-checked will get re-checked again.
    packetizer.clearPacket();
    TEST_ASSERT_EQUAL_INT(2, bus.available());  // (ulSize + 1) and (ulSize + 2)

    // Reset everthing
    memset(wasCalledAssert, false, sizeof(bool) * bufferSize);
    memset(endIndexAssert, 0, sizeof(int) * bufferSize);

    TEST_ASSERT_EQUAL_INT(0, packetizer.recheckBitmap);

    // Call our hasPacket method
    TEST_ASSERT_FALSE(packetizer.hasPacket());
    TEST_ASSERT_EQUAL_INT(0, packetizer.packetLength());

    TEST_ASSERT_TRUE_MESSAGE(wasCalledAssert[0], "isPacket was not called again on the first byte when it should have been");
    TEST_ASSERT_EQUAL_INT(0, endIndexAssert[0]);

    TEST_ASSERT_EQUAL_INT(1, bus.available());  // Only (ulSize + 2) is on the bus now
    TEST_ASSERT_EQUAL_INT(ulSize + 2, bus[0]);
    TEST_ASSERT_EQUAL_INT(-1, bus[1]);
  }

  void run_tests() {
    RUN_TEST(test_by_default_no_packets_are_available);
    RUN_TEST(test_can_get_simple_packet);
    RUN_TEST(test_only_no_bytes_get_skipped);
    RUN_TEST(test_not_enough_bytes_get_skipped_if_buffer_is_full);
    RUN_TEST(test_no_bytes_do_not_get_tested_again);
  }
}

#endif