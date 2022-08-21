#ifdef UNIT_TEST
#include <unity.h>

#include "assertable_buffer.h"
#include "matching_bytes.h"
#include "rs485bus.hpp"
#include "packetizer.h"
#include "fakeit/fakeit.hpp"

using namespace fakeit;

namespace PacketizerReadTest {
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
    When(Method(ArduinoFake(), millis)).AlwaysReturn(0);

    TEST_ASSERT_FALSE(packetizer.hasPacket());
    TEST_ASSERT_EQUAL_INT(0, packetizer.packetLength());
  }

  void test_can_get_simple_packet() {
    AssertableBuffer buffer;
    setUpArduinoFake();
    RS485Bus<8> bus(buffer, readEnablePin, writeEnablePin);
    PacketMatchingBytes packetInfo;
    Packetizer packetizer(bus, packetInfo);
    When(Method(ArduinoFake(), millis)).AlwaysReturn(0);

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
    When(Method(ArduinoFake(), millis)).AlwaysReturn(0);

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
    When(Method(ArduinoFake(), millis)).AlwaysReturn(0);

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
    constexpr size_t ulSize = sizeof(unsigned long) * 8;  // Needs to match the date type used for recheckBitmap in Packetizer
    constexpr int bufferSize = ulSize + 5;  // unsigned long size + 3 bytes we add + 2 just to be safe

    AssertableBuffer buffer;
    setUpArduinoFake();
    RS485Bus<bufferSize> bus(buffer, readEnablePin, writeEnablePin);
    PacketMatchingBytes packetInfo;
    Mock<PacketMatchingBytes> spy = spyMatchingBytes(packetInfo);
    Packetizer packetizer(bus, packetInfo);
    When(Method(ArduinoFake(), millis)).AlwaysReturn(0);

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

    // -- Nothing should have changed, but we want to verify we can still see the packet with nothing having been called

    // Reset everthing
    memset(wasCalledAssert, false, sizeof(bool) * bufferSize);
    memset(endIndexAssert, 0, sizeof(int) * bufferSize);

    // Call our hasPacket method
    TEST_ASSERT_TRUE(packetizer.hasPacket());
    TEST_ASSERT_EQUAL_INT(3, packetizer.packetLength());
    TEST_ASSERT_EQUAL_INT(5, bus.available());  // Everything else got cleared out but the packet and our two extra bytes

    // Verify nothing was called, meaning the packetizer cached the reuslt from last time
    for(int i = 0; i < ulSize; i++) {
      message = "Was called for start index " + std::to_string(i) + " when it should not have been";
      TEST_ASSERT_FALSE_MESSAGE(wasCalledAssert[i], message.c_str());

      message = "Bad end index for start index " + std::to_string(i);
      TEST_ASSERT_EQUAL_INT_MESSAGE(0, endIndexAssert[i], message.c_str());
    }

    // -- Clearing the packet will result in (ulSize + 1) and (ulSize + 2) being on the bus. The first is "no" so goes away. We do this step to make sure previous times where things were not re-checked will get re-checked again.
    packetizer.clearPacket();
    TEST_ASSERT_EQUAL_INT(2, bus.available());  // (ulSize + 1) and (ulSize + 2)

    // Reset everthing
    memset(wasCalledAssert, false, sizeof(bool) * bufferSize);
    memset(endIndexAssert, 0, sizeof(int) * bufferSize);

    // Call our hasPacket method
    TEST_ASSERT_FALSE(packetizer.hasPacket());
    TEST_ASSERT_EQUAL_INT(0, packetizer.packetLength());

    TEST_ASSERT_TRUE_MESSAGE(wasCalledAssert[0], "isPacket was not called again on the first byte when it should have been");
    TEST_ASSERT_EQUAL_INT(0, endIndexAssert[0]);

    TEST_ASSERT_EQUAL_INT(1, bus.available());  // Only (ulSize + 2) is on the bus now
    TEST_ASSERT_EQUAL_INT(ulSize + 2, bus[0]);
    TEST_ASSERT_EQUAL_INT(-1, bus[1]);
  }

  void test_no_bytes_past_our_limit_get_rechecked() {
    constexpr size_t ulSize = sizeof(unsigned long) * 8;  // Needs to match the date type used for recheckBitmap in Packetizer
    constexpr int bufferSize = ulSize + 3;  // unsigned long size + 1 byte we add + 2 just to be safe

    AssertableBuffer buffer;
    setUpArduinoFake();
    RS485Bus<bufferSize> bus(buffer, readEnablePin, writeEnablePin);
    PacketMatchingBytes packetInfo;
    Mock<PacketMatchingBytes> spy = spyMatchingBytes(packetInfo);
    Packetizer packetizer(bus, packetInfo);
    When(Method(ArduinoFake(), millis)).AlwaysReturn(0);

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

    // Even values of i is "not enough bytes", odd is "no" and we want both. (2 * i + 1) will always be odd. We want ulSize+1 bytes of that.
    buffer << 0; // Start with an even byte so we don't automatically shift all the "no" answers away.
    for(int i = 0; i < ulSize; i++) {
      buffer << 2 * i + 1;
    }
    
    // Ensure the bus hasn't fetched a single byte from the serial bus yet, meaning the packetizer calls fetch
    TEST_ASSERT_EQUAL_INT(0, bus.available());
    TEST_ASSERT_EQUAL_INT(0, packetizer.packetLength());

    TEST_ASSERT_FALSE(packetizer.hasPacket());
    TEST_ASSERT_EQUAL_INT(0, packetizer.packetLength());
    TEST_ASSERT_EQUAL_INT(ulSize + 1, bus.available());

    // -- We want to verify that "isPacket" got called on everything

    std::string message;
    for(int i = 0; i < bus.bufferSize(); i++) {
      if(i <= ulSize) {
        message = "Was not called for start index " + std::to_string(i);
        TEST_ASSERT_TRUE_MESSAGE(wasCalledAssert[i], message.c_str());

        message = "Bad end index for start index " + std::to_string(i);
        TEST_ASSERT_EQUAL_INT_MESSAGE(ulSize, endIndexAssert[i], message.c_str());
      } else {
        message = "Was called for start index " + std::to_string(i);
        TEST_ASSERT_FALSE_MESSAGE(wasCalledAssert[i], message.c_str());

        message = "Bad end index for start index " + std::to_string(i);
        TEST_ASSERT_EQUAL_INT_MESSAGE(0, endIndexAssert[i], message.c_str());
      }
    }

    // -- Now we want to call hasPacket again and verify that the NO past ulSize gets called on again.

    // Queue a byte that will get checked regardless. Otherwise hasPacket will exit without rechecking anything
    buffer << 255;

    // Reset everthing
    memset(wasCalledAssert, false, sizeof(bool) * bufferSize);
    memset(endIndexAssert, 0, sizeof(int) * bufferSize);

    // Call our hasPacket method
    TEST_ASSERT_FALSE(packetizer.hasPacket());
    TEST_ASSERT_EQUAL_INT(0, packetizer.packetLength());
    TEST_ASSERT_EQUAL_INT(ulSize + 2, bus.available());

    // Verify that only bytes at (0), (ulSize), and (ulSize + 1) were checked.
    // (ulSize + 1) wasn't ever checked, (0) is our "not enough bytes", and (ulSize) is past where we're able to check it.
    for(int i = 0; i < bus.bufferSize(); i++) {
      if(i == 0 || i == ulSize || i == (ulSize + 1)) {
        message = "Was not called for start index " + std::to_string(i);
        TEST_ASSERT_TRUE_MESSAGE(wasCalledAssert[i], message.c_str());

        message = "Bad end index for start index " + std::to_string(i);
        TEST_ASSERT_EQUAL_INT_MESSAGE(ulSize + 1, endIndexAssert[i], message.c_str());
      } else {
        message = "Was called for start index " + std::to_string(i);
        TEST_ASSERT_FALSE_MESSAGE(wasCalledAssert[i], message.c_str());

        message = "Bad end index for start index " + std::to_string(i);
        TEST_ASSERT_EQUAL_INT_MESSAGE(0, endIndexAssert[i], message.c_str());
      }
    }
  }

  void test_can_get_packet_even_if_it_is_past_bus_size_if_bytes_are_shifted() {
    AssertableBuffer buffer;
    setUpArduinoFake();
    RS485Bus<3> bus(buffer, readEnablePin, writeEnablePin);
    PacketMatchingBytes packetInfo;
    Packetizer packetizer(bus, packetInfo);
    When(Method(ArduinoFake(), millis)).AlwaysReturn(0);

    // This format and bus size is specifically so already checked "no" bytes do get removed if they're at the beginning
    buffer << 0x01 << 0x02 << 0x03 << 0x04 << 0x06 << 0x06;
    /*
    This is the worked out explanation. neb == "not enough bytes".

    01 02 03 | first fetch
    02 03 -- | "no" gets shifted, lastBusAvailable becomes 2.
    02 03 -- | "neb" stays, buffer isn't full. 03 gets a no.
    02 03 04 | fetch is called. lastBusAvailable (2) != bus available (3), so continue with lastBusAvailable as 3. 0x2 and 0x4 are checked
    03 04 -- | buffer is full, so "neb" is removed. lastBusAvailable becomes 2
    04 -- -- | "no" gets shifted, lastBusAvailable becomes 1.
    04 -- -- | "neb" stays, buffer isn't full.
    04 06 06 | fetch is called. lastBusAvailable (1) != bus available (3), so continue with lastBusAvailable as 3.
    06 06 -- | "neb" 04 is removed since the buffer is full
    06 06 -- | yes is found! 0x06 <-> 0x06 is our packet
    */

    // Ensure the bus hasn't fetched a single byte from the serial bus yet, meaning the packetizer calls fetch
    TEST_ASSERT_EQUAL_INT(0, bus.available());
    TEST_ASSERT_EQUAL_INT(0, packetizer.packetLength());

    TEST_ASSERT_TRUE(packetizer.hasPacket());
    TEST_ASSERT_EQUAL_INT(2, packetizer.packetLength());

    TEST_ASSERT_EQUAL_INT(2, bus.available());
    TEST_ASSERT_EQUAL_INT(0x06, bus[0]);
    TEST_ASSERT_EQUAL_INT(0x06, bus[1]);
    TEST_ASSERT_EQUAL_INT(-1, bus[2]);

    packetizer.clearPacket();
    TEST_ASSERT_FALSE(packetizer.hasPacket());
    TEST_ASSERT_EQUAL_INT(0, packetizer.packetLength());
    TEST_ASSERT_EQUAL_INT(0, bus.available());
    TEST_ASSERT_EQUAL_INT(-1, bus[0]);
  }
  
  /**
   * This is the same setup as the test above "test_can_get_packet_even_if_it_is_past_bus_size_if_bytes_are_shifted".
   * The big difference is that fetch must be called 4 times to get our packet whereas we make sure it "times out" after
   * 3 calls.
   */
  void test_cannot_get_packet_if_timeout_is_reached() {
    AssertableBuffer buffer;
    setUpArduinoFake();
    RS485Bus<3> bus(buffer, readEnablePin, writeEnablePin);
    PacketMatchingBytes packetInfo;
    Packetizer packetizer(bus, packetInfo);
    packetizer.setMaxReadTimeout(200);

    buffer << 0x01 << 0x02 << 0x03 << 0x04 << 0x06 << 0x06;
    When(Method(ArduinoFake(), millis)).Return(
      0,    // Start time ms
      10,   // fetch from bus time
      100,  // Timeout check
      110,  // fetch from bus time
      200   // Timeout check
      );

    // Ensure the bus hasn't fetched a single byte from the serial bus yet, meaning the packetizer calls fetch
    TEST_ASSERT_EQUAL_INT(0, bus.available());
    TEST_ASSERT_EQUAL_INT(0, packetizer.packetLength());

    TEST_ASSERT_FALSE(packetizer.hasPacket());
    TEST_ASSERT_EQUAL_INT(0, packetizer.packetLength());

    TEST_ASSERT_EQUAL_INT(2, bus.available());
    TEST_ASSERT_EQUAL_INT(0x04, bus[0]);
    TEST_ASSERT_EQUAL_INT(0x06, bus[1]);
    TEST_ASSERT_EQUAL_INT(-1, bus[2]);
  }

  void run_tests() {
    RUN_TEST(test_by_default_no_packets_are_available);
    RUN_TEST(test_can_get_simple_packet);
    RUN_TEST(test_only_no_bytes_get_skipped);
    RUN_TEST(test_not_enough_bytes_get_skipped_if_buffer_is_full);
    RUN_TEST(test_no_bytes_do_not_get_tested_again);
    RUN_TEST(test_no_bytes_past_our_limit_get_rechecked);
    RUN_TEST(test_can_get_packet_even_if_it_is_past_bus_size_if_bytes_are_shifted);
    RUN_TEST(test_cannot_get_packet_if_timeout_is_reached);
  }
}

#endif