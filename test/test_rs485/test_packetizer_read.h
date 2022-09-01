#pragma once

#include "../assertable_buffer.h"
#include "../matching_bytes.h"
#include "../fixtures.h"

#include <gtest/gtest.h>
#include "fakeit/fakeit.hpp"

#include "rs485/rs485bus.hpp"
#include "rs485/packetizer.h"

using namespace fakeit;

class PacketizerReadTest : public PrepBus {
protected:
  PacketizerReadTest(): PrepBus(),
    protocolSpy(protocol) {};

  void SetUp() {
    When(Method(ArduinoFake(), millis)).AlwaysReturn(0);

    ArduinoFake().ClearInvocationHistory();
  };

  AssertableBuffer buffer;
  ProtocolMatchingBytes protocol;
  Mock<ProtocolMatchingBytes> protocolSpy;
};

class PacketizerReadBusTest : public PacketizerReadTest {
protected:
  PacketizerReadBusTest(): PacketizerReadTest(),
    bus(buffer, readEnablePin, writeEnablePin),
    packetizer(bus, protocol) {}

  RS485Bus<8> bus;
  Packetizer packetizer;
};

class PacketizerReadBus3Test : public PacketizerReadTest {
protected:
  PacketizerReadBus3Test(): PacketizerReadTest(),
    bus(buffer, readEnablePin, writeEnablePin),
    packetizer(bus, protocol) {}

  RS485Bus<3> bus;
  Packetizer packetizer;
};

class PacketizerReadBusBigTest : public PacketizerReadTest {
protected:
  PacketizerReadBusBigTest(): PacketizerReadTest(),
    bus(buffer, readEnablePin, writeEnablePin),
    packetizer(bus, protocol) {}

  constexpr static int ulSize = sizeof(unsigned long) * 8;  // Needs to match the date type used for recheckBitmap in Packetizer
  constexpr static int bufferSize = ulSize + 20;  // unsigned long size + 20 should be good enough for all the tests
  RS485Bus<bufferSize> bus;
  Packetizer packetizer;
};

TEST_F(PacketizerReadBusTest, by_default_no_packets_are_available) {
  EXPECT_FALSE(packetizer.hasPacket());
  EXPECT_EQ(packetizer.packetLength(), 0);
}

TEST_F(PacketizerReadBusTest, can_get_simple_packet) {
  buffer << 0x01 << 0x02 << 0x03 << 0x02 << 0x04;

  // Ensure the bus hasn't fetched a single byte from the serial bus yet, meaning the packetizer calls fetch
  EXPECT_EQ(bus.available(), 0);
  EXPECT_EQ(packetizer.packetLength(), 0);

  EXPECT_TRUE(packetizer.hasPacket());
  EXPECT_EQ(packetizer.packetLength(), 3);

  EXPECT_EQ(bus.available(), 4); // The first byte isn't part of the packet
  EXPECT_EQ(bus[0], 0x02);
  EXPECT_EQ(bus[1], 0x03);
  EXPECT_EQ(bus[2], 0x02);
  EXPECT_EQ(bus[3], 0x04);
  EXPECT_EQ(bus[4], -1);

  packetizer.clearPacket();
  EXPECT_FALSE(packetizer.hasPacket());
  EXPECT_EQ(packetizer.packetLength(), 0);
  EXPECT_EQ(bus.available(), 1);
  EXPECT_EQ(bus[0], 0x04);
  EXPECT_EQ(bus[1], -1);
}

TEST_F(PacketizerReadBusTest, only_no_bytes_get_skipped) {
  buffer << 0x01 << 0x03 << 0x02 << 0x05;

  // Ensure the bus hasn't fetched a single byte from the serial bus yet, meaning the packetizer calls fetch
  EXPECT_EQ(bus.available(), 0);
  EXPECT_EQ(packetizer.packetLength(), 0);

  EXPECT_FALSE(packetizer.hasPacket());
  EXPECT_EQ(packetizer.packetLength(), 0);

  EXPECT_EQ(bus.available(), 2);
  EXPECT_EQ(bus[0], 0x02);  // Returns "not enough bytes", so isn't discarded
  EXPECT_EQ(bus[1], 0x05);  // Returns "no"
  EXPECT_EQ(bus[2], -1);
}

TEST_F(PacketizerReadBus3Test, not_enough_bytes_get_skipped_if_buffer_is_full) {
  // All of these values return "not enough bytes", but only 0x02 will be skipped
  buffer << 0x02 << 0x04 << 0x6;

  // Ensure the bus hasn't fetched a single byte from the serial bus yet, meaning the packetizer calls fetch
  EXPECT_EQ(bus.available(), 0);
  EXPECT_EQ(packetizer.packetLength(), 0);

  EXPECT_FALSE(packetizer.hasPacket());
  EXPECT_EQ(packetizer.packetLength(), 0);

  EXPECT_EQ(bus.available(), 2);
  EXPECT_EQ(bus[0], 0x04);
  EXPECT_EQ(bus[1], 0x06);
  EXPECT_EQ(bus[2], -1);
}

/**
 * This test is pretty gnarly. There's a couple of reasons for that. The first is the "hack" below to workaround verifying captured values.
 * The second has to do with the flow for the packetizer. This more closely resembles what actual usage would look like and it found some
 * issues that splitting up the test cases did not.
 *
 * Because it's long and hard to read, the verification sections are broken up by comments that start with "--".
 */
TEST_F(PacketizerReadBusBigTest, no_bytes_do_not_get_tested_again) {
  constexpr size_t ulSize = PacketizerReadBusBigTest::ulSize;

  // AWFUL HACK ALERT - related: https://github.com/eranpeer/FakeIt/issues/274
  // Essentially, we can't verify captured arguments. So we have an array to verify if we've called in using the correct parameters.
  // We also don't want to re-implement what ProtocolMatchingBytes does so we create a new instance that we defer to, since our main instance is being spied on.
  bool wasCalledAssert[bufferSize] = { false };
  int endIndexAssert[bufferSize] = { 0 };
  ProtocolMatchingBytes baseProtocol;
  When(Method(protocolSpy, isPacket)).AlwaysDo([&](const RS485BusBase& bus, const int startIndex, int& endIndex)->PacketStatus {
    wasCalledAssert[startIndex] = true;
    PacketStatus result = baseProtocol.isPacket(bus, startIndex, endIndex);

    endIndexAssert[startIndex] = endIndex;
    return result;
  });

  for(int i = 0; i < ulSize; i++) {
    // Even values of i is "not enough bytes", odd is "no" and we want both. We can't just alternate or we'll get a valid packet, though. We also need it to start with "not enough bytes" so nothing gets shifted.
    buffer << i;
  }
  
  // Ensure the bus hasn't fetched a single byte from the serial bus yet, meaning the packetizer calls fetch
  EXPECT_EQ(bus.available(), 0);
  EXPECT_EQ(packetizer.packetLength(), 0);

  EXPECT_FALSE(packetizer.hasPacket());
  EXPECT_EQ(packetizer.packetLength(), 0);
  EXPECT_EQ(bus.available(), ulSize);

  // -- We want to verify that "isPacket" got called on everything

  for(int i = 0; i < ulSize; i++) {
    EXPECT_TRUE(wasCalledAssert[i]) << "Was not called for start index " << i;

    EXPECT_EQ(endIndexAssert[i], ulSize - 1) << "Bad end index for start index " << i;
  }

  // -- Now we want to call hasPacket again and verify that nothing gets called because no new bytes became available

  // Reset everthing
  memset(wasCalledAssert, false, sizeof(bool) * bufferSize);
  memset(endIndexAssert, 0, sizeof(int) * bufferSize);

  // Call our hasPacket method
  EXPECT_FALSE(packetizer.hasPacket());
  EXPECT_EQ(packetizer.packetLength(), 0);
  EXPECT_EQ(bus.available(), ulSize);

  // And finally, verify
  for(int i = 0; i < ulSize; i++) {
    EXPECT_FALSE(wasCalledAssert[i]) << "Was called for start index " << i << " when it should not have been";

    EXPECT_EQ(endIndexAssert[i], 0) << "Bad end index for start index " << i;
  }

  // -- We want to complete a packet that was "not enough bytes" before. Since the size of a long will always be even, we match with size - 2. (size - 1) is currently the last byte on the buffer.

  // Queue up an even number, plus a "no", plus a "not enough bytes"
  int validPacketByte = ulSize - 2;
  buffer << validPacketByte << (ulSize + 1) << (ulSize + 2);

  // Reset everthing
  memset(wasCalledAssert, false, sizeof(bool) * bufferSize);
  memset(endIndexAssert, 0, sizeof(int) * bufferSize);

  // Call our hasPacket method
  EXPECT_TRUE(packetizer.hasPacket());
  EXPECT_EQ(packetizer.packetLength(), 3);
  EXPECT_EQ(bus.available(), 5);  // Everything else got cleared out but the packet and our two extra bytes

  // And finally, verify
  for(int i = 0; i < ulSize; i++) {
    if(i % 2 == 0) {
      EXPECT_TRUE(wasCalledAssert[i]) << "Was not called for start index " << i;

      if(i == validPacketByte) {
        EXPECT_EQ(endIndexAssert[i], ulSize) << "Bad end index for start index " << i;
      } else {
        EXPECT_EQ(endIndexAssert[i], ulSize + 2) << "Bad end index for start index " << i;
      }
    } else {
      EXPECT_FALSE(wasCalledAssert[i]) << "Was called for start index " << i << " when it should not have been";

      EXPECT_EQ(endIndexAssert[i], 0) << "Bad end index for start index " << i;
    }
  }

  EXPECT_FALSE(wasCalledAssert[ulSize]) << "No bytes after valid packet should have been tested";
  EXPECT_FALSE(wasCalledAssert[ulSize+1]) << "No bytes after valid packet should have been tested";
  EXPECT_FALSE(wasCalledAssert[ulSize+2]) << "No bytes after valid packet should have been tested";

  // -- Nothing should have changed, but we want to verify we can still see the packet with nothing having been called

  // Reset everthing
  memset(wasCalledAssert, false, sizeof(bool) * bufferSize);
  memset(endIndexAssert, 0, sizeof(int) * bufferSize);

  // Call our hasPacket method
  EXPECT_TRUE(packetizer.hasPacket());
  EXPECT_EQ(packetizer.packetLength(), 3);
  EXPECT_EQ(bus.available(), 5);  // Everything else got cleared out but the packet and our two extra bytes

  // Verify nothing was called, meaning the packetizer cached the reuslt from last time
  for(int i = 0; i < ulSize; i++) {
    EXPECT_FALSE(wasCalledAssert[i]) << "Was called for start index " << i << " when it should not have been";

    EXPECT_EQ(endIndexAssert[i], 0) << "Bad end index for start index " << i;
  }

  // -- Clearing the packet will result in (ulSize + 1) and (ulSize + 2) being on the bus. The first is "no" so goes away. We do this step to make sure previous times where things were not re-checked will get re-checked again.
  packetizer.clearPacket();
  EXPECT_EQ(bus.available(), 2);  // (ulSize + 1) and (ulSize + 2)

  // Reset everthing
  memset(wasCalledAssert, false, sizeof(bool) * bufferSize);
  memset(endIndexAssert, 0, sizeof(int) * bufferSize);

  // Call our hasPacket method
  EXPECT_FALSE(packetizer.hasPacket());
  EXPECT_EQ(packetizer.packetLength(), 0);

  EXPECT_TRUE(wasCalledAssert[0]) << "isPacket was not called again on the first byte when it should have been";
  EXPECT_EQ(endIndexAssert[0], 0);

  EXPECT_EQ(bus.available(), 1);  // Only (ulSize + 2) is on the bus now
  EXPECT_EQ(bus[0], ulSize + 2);
  EXPECT_EQ(bus[1], -1);
}

TEST_F(PacketizerReadBusBigTest, no_bytes_past_our_limit_get_rechecked) {
  constexpr size_t ulSize = PacketizerReadBusBigTest::ulSize;

  // AWFUL HACK ALERT - related: https://github.com/eranpeer/FakeIt/issues/274
  // Essentially, we can't verify captured arguments. So we have an array to verify if we've called in using the correct parameters.
  // We also don't want to re-implement what ProtocolMatchingBytes does so we create a new instance that we defer to, since our main instance is being spied on.
  bool wasCalledAssert[bufferSize] = { false };
  int endIndexAssert[bufferSize] = { 0 };
  ProtocolMatchingBytes baseProtocol;
  When(Method(protocolSpy, isPacket)).AlwaysDo([&](const RS485BusBase& bus, const int& startIndex, int& endIndex)->PacketStatus {
    wasCalledAssert[startIndex] = true;
    PacketStatus result = baseProtocol.isPacket(bus, startIndex, endIndex);

    endIndexAssert[startIndex] = endIndex;
    return result;
  });

  // Even values of i is "not enough bytes", odd is "no" and we want both. (2 * i + 1) will always be odd. We want ulSize+1 bytes of that.
  buffer << 0; // Start with an even byte so we don't automatically shift all the "no" answers away.
  for(int i = 0; i < ulSize; i++) {
    buffer << 2 * i + 1;
  }
  
  // Ensure the bus hasn't fetched a single byte from the serial bus yet, meaning the packetizer calls fetch
  EXPECT_EQ(bus.available(), 0);
  EXPECT_EQ(packetizer.packetLength(), 0);

  EXPECT_FALSE(packetizer.hasPacket());
  EXPECT_EQ(packetizer.packetLength(), 0);
  EXPECT_EQ(bus.available(), ulSize + 1);

  // -- We want to verify that "isPacket" got called on everything

  std::string message;
  for(int i = 0; i < bus.bufferSize(); i++) {
    if(i <= ulSize) {
      EXPECT_TRUE(wasCalledAssert[i]) << "Was not called for start index " << i;

      EXPECT_EQ(endIndexAssert[i], ulSize) << "Bad end index for start index " << i;
    } else {
      EXPECT_FALSE(wasCalledAssert[i]) << "Was called for start index " << i;

      EXPECT_EQ(endIndexAssert[i], 0) << "Bad end index for start index " << i;
    }
  }

  // -- Now we want to call hasPacket again and verify that the NO past ulSize gets called on again.

  // Queue a byte that will get checked regardless. Otherwise hasPacket will exit without rechecking anything
  buffer << 255;

  // Reset everthing
  memset(wasCalledAssert, false, sizeof(bool) * bufferSize);
  memset(endIndexAssert, 0, sizeof(int) * bufferSize);

  // Call our hasPacket method
  EXPECT_FALSE(packetizer.hasPacket());
  EXPECT_EQ(packetizer.packetLength(), 0);
  EXPECT_EQ(bus.available(), ulSize + 2);

  // Verify that only bytes at (0), (ulSize), and (ulSize + 1) were checked.
  // (ulSize + 1) wasn't ever checked, (0) is our "not enough bytes", and (ulSize) is past where we're able to check it.
  for(int i = 0; i < bus.bufferSize(); i++) {
    if(i == 0 || i == ulSize || i == (ulSize + 1)) {
      EXPECT_TRUE(wasCalledAssert[i]) << "Was not called for start index " << i;

      EXPECT_EQ(endIndexAssert[i], ulSize + 1) << "Bad end index for start index " << i;
    } else {
      EXPECT_FALSE(wasCalledAssert[i]) << "Was called for start index " << i;

      EXPECT_EQ(endIndexAssert[i], 0) << "Bad end index for start index " << i;
    }
  }
}

TEST_F(PacketizerReadBus3Test, can_get_packet_even_if_it_is_past_bus_size_if_bytes_are_shifted) {
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
  EXPECT_EQ(bus.available(), 0);
  EXPECT_EQ(packetizer.packetLength(), 0);

  EXPECT_TRUE(packetizer.hasPacket());
  EXPECT_EQ(packetizer.packetLength(), 2);

  EXPECT_EQ(bus.available(), 2);
  EXPECT_EQ(bus[0], 0x06);
  EXPECT_EQ(bus[1], 0x06);
  EXPECT_EQ(bus[2], -1);

  packetizer.clearPacket();
  EXPECT_FALSE(packetizer.hasPacket());
  EXPECT_EQ(packetizer.packetLength(), 0);
  EXPECT_EQ(bus.available(), 0);
  EXPECT_EQ(bus[0], -1);
}

/**
 * This is the same setup as the test above "test_can_get_packet_even_if_it_is_past_bus_size_if_bytes_are_shifted".
 * The big difference is that fetch must be called 4 times to get our packet whereas we make sure it "times out" after
 * 3 calls.
 */
TEST_F(PacketizerReadBus3Test, cannot_get_packet_if_timeout_is_reached) {
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
  EXPECT_EQ(bus.available(), 0);
  EXPECT_EQ(packetizer.packetLength(), 0);

  EXPECT_FALSE(packetizer.hasPacket());
  EXPECT_EQ(packetizer.packetLength(), 0);

  EXPECT_EQ(bus.available(), 2);
  EXPECT_EQ(bus[0], 0x04);
  EXPECT_EQ(bus[1], 0x06);
  EXPECT_EQ(bus[2], -1);
}