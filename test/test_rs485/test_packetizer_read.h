#pragma once

#include "../assertable_bus_io.hpp"
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

  AssertableBusIO busIO;
  ProtocolMatchingBytes protocol;
  Mock<ProtocolMatchingBytes> protocolSpy;
};

class PacketizerReadBusTest : public PacketizerReadTest {
protected:
  PacketizerReadBusTest(): PacketizerReadTest(),
    bus(busIO, readEnablePin, writeEnablePin),
    packetizer(bus, protocol) {}

  RS485Bus<8> bus;
  Packetizer packetizer;
};

class PacketizerReadBus3Test : public PacketizerReadTest {
protected:
  PacketizerReadBus3Test(): PacketizerReadTest(),
    bus(busIO, readEnablePin, writeEnablePin),
    packetizer(bus, protocol) {}

  RS485Bus<3> bus;
  Packetizer packetizer;
};

class PacketizerReadBusBigTest : public PacketizerReadTest {
protected:
  PacketizerReadBusBigTest(): PacketizerReadTest(),
    bus(busIO, readEnablePin, writeEnablePin),
    packetizer(bus, protocol) {}

  constexpr static size_t u64Size = sizeof(uint64_t) * 8;  // Needs to match the date type used for recheckBitmap in Packetizer
  constexpr static size_t bufferSize = u64Size + 20;  // uint64_t size + 20 should be good enough for all the tests
  RS485Bus<bufferSize> bus;
  Packetizer packetizer;
};

TEST_F(PacketizerReadBusTest, by_default_no_packets_are_available) {
  EXPECT_FALSE(packetizer.hasPacket());
  EXPECT_EQ(0, packetizer.packetLength());
}

TEST_F(PacketizerReadBusTest, can_get_simple_packet) {
  busIO << 0x01 << 0x02 << 0x03 << 0x02 << 0x04;

  // Ensure the bus hasn't fetched a single byte from the serial bus yet, meaning the packetizer calls fetch
  EXPECT_EQ(0, bus.available());
  EXPECT_EQ(0, packetizer.packetLength());

  EXPECT_TRUE(packetizer.hasPacket());
  EXPECT_EQ(3, packetizer.packetLength());

  EXPECT_EQ(4, bus.available()); // The first byte isn't part of the packet
  EXPECT_EQ(0x02, bus[0]);
  EXPECT_EQ(0x03, bus[1]);
  EXPECT_EQ(0x02, bus[2]);
  EXPECT_EQ(0x04, bus[3]);
  EXPECT_EQ(-1, bus[4]);

  packetizer.clearPacket();
  EXPECT_FALSE(packetizer.hasPacket());
  EXPECT_EQ(0, packetizer.packetLength());
  EXPECT_EQ(1, bus.available());
  EXPECT_EQ(0x04, bus[0]);
  EXPECT_EQ(-1, bus[1]);
}

TEST_F(PacketizerReadBusTest, only_no_bytes_get_skipped) {
  busIO << 0x01 << 0x03 << 0x02 << 0x05;

  // Ensure the bus hasn't fetched a single byte from the serial bus yet, meaning the packetizer calls fetch
  EXPECT_EQ(0, bus.available());
  EXPECT_EQ(0, packetizer.packetLength());

  EXPECT_FALSE(packetizer.hasPacket());
  EXPECT_EQ(0, packetizer.packetLength());

  EXPECT_EQ(2, bus.available());
  EXPECT_EQ(0x02, bus[0]);  // Returns "not enough bytes", so isn't discarded
  EXPECT_EQ(0x05, bus[1]);  // Returns "no"
  EXPECT_EQ(-1, bus[2]);
}

TEST_F(PacketizerReadBus3Test, not_enough_bytes_get_skipped_if_buffer_is_full) {
  // All of these values return "not enough bytes", but only 0x02 will be skipped
  busIO << 0x02 << 0x04 << 0x6;

  // Ensure the bus hasn't fetched a single byte from the serial bus yet, meaning the packetizer calls fetch
  EXPECT_EQ(0, bus.available());
  EXPECT_EQ(0, packetizer.packetLength());

  EXPECT_FALSE(packetizer.hasPacket());
  EXPECT_EQ(0, packetizer.packetLength());

  EXPECT_EQ(2, bus.available());
  EXPECT_EQ(0x04, bus[0]);
  EXPECT_EQ(0x06, bus[1]);
  EXPECT_EQ(-1, bus[2]);
}

/**
 * This test is pretty gnarly. There's a couple of reasons for that. The first is the "hack" below to workaround verifying captured values.
 * The second has to do with the flow for the packetizer. This more closely resembles what actual usage would look like and it found some
 * issues that splitting up the test cases did not.
 *
 * Because it's long and hard to read, the verification sections are broken up by comments that start with "--".
 */
TEST_F(PacketizerReadBusBigTest, no_bytes_do_not_get_tested_again) {
  constexpr size_t u64Size = PacketizerReadBusBigTest::u64Size;

  // AWFUL HACK ALERT - related: https://github.com/eranpeer/FakeIt/issues/274
  // Essentially, we can't verify captured arguments. So we have an array to verify if we've called in using the correct parameters.
  // We also don't want to re-implement what ProtocolMatchingBytes does so we create a new instance that we defer to, since our main instance is being spied on.
  bool wasCalledAssert[bufferSize] = { false };
  size_t endIndexAssert[bufferSize] = { 0 };
  ProtocolMatchingBytes baseProtocol;
  When(Method(protocolSpy, isPacket)).AlwaysDo([&](const RS485BusBase& bus, size_t startIndex, size_t& endIndex)->PacketStatus {
    wasCalledAssert[startIndex] = true;
    PacketStatus result = baseProtocol.isPacket(bus, startIndex, endIndex);

    endIndexAssert[startIndex] = endIndex;
    return result;
  });

  for(size_t i = 0; i < u64Size; i++) {
    // Even values of i is "not enough bytes", odd is "no" and we want both. We can't just alternate or we'll get a valid packet, though. We also need it to start with "not enough bytes" so nothing gets shifted.
    busIO << i;
  }
  
  // Ensure the bus hasn't fetched a single byte from the serial bus yet, meaning the packetizer calls fetch
  EXPECT_EQ(0, bus.available());
  EXPECT_EQ(0, packetizer.packetLength());

  EXPECT_FALSE(packetizer.hasPacket());
  EXPECT_EQ(0, packetizer.packetLength());
  EXPECT_EQ(u64Size, bus.available());

  // -- We want to verify that "isPacket" got called on everything

  for(size_t i = 0; i < u64Size; i++) {
    EXPECT_TRUE(wasCalledAssert[i]) << "Was not called for start index " << i;

    EXPECT_EQ(endIndexAssert[i], u64Size - 1) << "Bad end index for start index " << i;
  }

  // -- Now we want to call hasPacket again and verify that nothing gets called because no new bytes became available

  // Reset everthing
  memset(wasCalledAssert, false, sizeof(wasCalledAssert));
  memset(endIndexAssert, 0, sizeof(endIndexAssert));

  // Call our hasPacket method
  EXPECT_FALSE(packetizer.hasPacket());
  EXPECT_EQ(0, packetizer.packetLength());
  EXPECT_EQ(u64Size, bus.available());

  // And finally, verify
  for(size_t i = 0; i < u64Size; i++) {
    EXPECT_FALSE(wasCalledAssert[i]) << "Was called for start index " << i << " when it should not have been";

    EXPECT_EQ(endIndexAssert[i], 0) << "Bad end index for start index " << i;
  }

  // -- We want to complete a packet that was "not enough bytes" before. Since the size of a long will always be even, we match with size - 2. (size - 1) is currently the last byte on the busIO.

  // Queue up an even number, plus a "no", plus a "not enough bytes"
  uint8_t validPacketByte = u64Size - 2;
  busIO << validPacketByte << (u64Size + 1) << (u64Size + 2);

  // Reset everthing
  memset(wasCalledAssert, false, sizeof(wasCalledAssert));
  memset(endIndexAssert, 0, sizeof(endIndexAssert));

  // Call our hasPacket method
  EXPECT_TRUE(packetizer.hasPacket());
  EXPECT_EQ(3, packetizer.packetLength());
  EXPECT_EQ(5, bus.available());  // Everything else got cleared out but the packet and our two extra bytes

  // And finally, verify
  for(size_t i = 0; i < u64Size; i++) {
    if(i % 2 == 0) {
      EXPECT_TRUE(wasCalledAssert[i]) << "Was not called for start index " << i;

      if(i == validPacketByte) {
        EXPECT_EQ(endIndexAssert[i], u64Size) << "Bad end index for start index " << i;
      } else {
        EXPECT_EQ(endIndexAssert[i], u64Size + 2) << "Bad end index for start index " << i;
      }
    } else {
      EXPECT_FALSE(wasCalledAssert[i]) << "Was called for start index " << i << " when it should not have been";

      EXPECT_EQ(endIndexAssert[i], 0) << "Bad end index for start index " << i;
    }
  }

  EXPECT_FALSE(wasCalledAssert[u64Size]) << "No bytes after valid packet should have been tested";
  EXPECT_FALSE(wasCalledAssert[u64Size+1]) << "No bytes after valid packet should have been tested";
  EXPECT_FALSE(wasCalledAssert[u64Size+2]) << "No bytes after valid packet should have been tested";

  // -- Nothing should have changed, but we want to verify we can still see the packet with nothing having been called

  // Reset everthing
  memset(wasCalledAssert, false, sizeof(wasCalledAssert));
  memset(endIndexAssert, 0, sizeof(endIndexAssert));

  // Call our hasPacket method
  EXPECT_TRUE(packetizer.hasPacket());
  EXPECT_EQ(3, packetizer.packetLength());
  EXPECT_EQ(5, bus.available());  // Everything else got cleared out but the packet and our two extra bytes

  // Verify nothing was called, meaning the packetizer cached the reuslt from last time
  for(size_t i = 0; i < u64Size; i++) {
    EXPECT_FALSE(wasCalledAssert[i]) << "Was called for start index " << i << " when it should not have been";

    EXPECT_EQ(endIndexAssert[i], 0) << "Bad end index for start index " << i;
  }

  // -- Clearing the packet will result in (u64Size + 1) and (u64Size + 2) being on the bus. The first is "no" so goes away. We do this step to make sure previous times where things were not re-checked will get re-checked again.
  packetizer.clearPacket();
  EXPECT_EQ(2, bus.available());  // (u64Size + 1) and (u64Size + 2)

  // Reset everthing
  memset(wasCalledAssert, false, sizeof(wasCalledAssert));
  memset(endIndexAssert, 0, sizeof(endIndexAssert));

  // Call our hasPacket method
  EXPECT_FALSE(packetizer.hasPacket());
  EXPECT_EQ(0, packetizer.packetLength());

  EXPECT_TRUE(wasCalledAssert[0]) << "isPacket was not called again on the first byte when it should have been";
  EXPECT_EQ(0, endIndexAssert[0]);

  EXPECT_EQ(1, bus.available());  // Only (u64Size + 2) is on the bus now
  EXPECT_EQ(u64Size + 2, bus[0]);
  EXPECT_EQ(-1, bus[1]);
}

TEST_F(PacketizerReadBusBigTest, no_bytes_past_our_limit_get_rechecked) {
  constexpr size_t u64Size = PacketizerReadBusBigTest::u64Size;

  // AWFUL HACK ALERT - related: https://github.com/eranpeer/FakeIt/issues/274
  // Essentially, we can't verify captured arguments. So we have an array to verify if we've called in using the correct parameters.
  // We also don't want to re-implement what ProtocolMatchingBytes does so we create a new instance that we defer to, since our main instance is being spied on.
  bool wasCalledAssert[bufferSize] = { false };
  size_t endIndexAssert[bufferSize] = { 0 };
  ProtocolMatchingBytes baseProtocol;
  When(Method(protocolSpy, isPacket)).AlwaysDo([&](const RS485BusBase& bus, size_t startIndex, size_t& endIndex)->PacketStatus {
    wasCalledAssert[startIndex] = true;
    PacketStatus result = baseProtocol.isPacket(bus, startIndex, endIndex);

    endIndexAssert[startIndex] = endIndex;
    return result;
  });

  // Even values of i is "not enough bytes", odd is "no" and we want both. (2 * i + 1) will always be odd. We want u64Size+1 bytes of that.
  busIO << 0; // Start with an even byte so we don't automatically shift all the "no" answers away.
  for(size_t i = 0; i < u64Size; i++) {
    busIO << 2 * i + 1;
  }
  
  // Ensure the bus hasn't fetched a single byte from the serial bus yet, meaning the packetizer calls fetch
  EXPECT_EQ(0, bus.available());
  EXPECT_EQ(0, packetizer.packetLength());

  EXPECT_FALSE(packetizer.hasPacket());
  EXPECT_EQ(0, packetizer.packetLength());
  EXPECT_EQ(u64Size + 1, bus.available());

  // -- We want to verify that "isPacket" got called on everything

  for(size_t i = 0; i < bus.bufferSize(); i++) {
    if(i <= u64Size) {
      EXPECT_TRUE(wasCalledAssert[i]) << "Was not called for start index " << i;

      EXPECT_EQ(endIndexAssert[i], u64Size) << "Bad end index for start index " << i;
    } else {
      EXPECT_FALSE(wasCalledAssert[i]) << "Was called for start index " << i;

      EXPECT_EQ(endIndexAssert[i], 0) << "Bad end index for start index " << i;
    }
  }

  // -- Now we want to call hasPacket again and verify that the NO past u64Size gets called on again.

  // Queue a byte that will get checked regardless. Otherwise hasPacket will exit without rechecking anything
  busIO << 255;

  // Reset everthing
  memset(wasCalledAssert, false, sizeof(wasCalledAssert));
  memset(endIndexAssert, 0, sizeof(endIndexAssert));

  // Call our hasPacket method
  EXPECT_FALSE(packetizer.hasPacket());
  EXPECT_EQ(0, packetizer.packetLength());
  EXPECT_EQ(u64Size + 2, bus.available());

  // Verify that only bytes at (0), (u64Size), and (u64Size + 1) were checked.
  // (u64Size + 1) wasn't ever checked, (0) is our "not enough bytes", and (u64Size) is past where we're able to check it.
  for(size_t i = 0; i < bus.bufferSize(); i++) {
    if(i == 0 || i == u64Size || i == (u64Size + 1)) {
      EXPECT_TRUE(wasCalledAssert[i]) << "Was not called for start index " << i;

      EXPECT_EQ(endIndexAssert[i], u64Size + 1) << "Bad end index for start index " << i;
    } else {
      EXPECT_FALSE(wasCalledAssert[i]) << "Was called for start index " << i;

      EXPECT_EQ(endIndexAssert[i], 0) << "Bad end index for start index " << i;
    }
  }
}

TEST_F(PacketizerReadBus3Test, can_get_packet_even_if_it_is_past_bus_size_if_bytes_are_shifted) {
  // This format and bus size is specifically so already checked "no" bytes do get removed if they're at the beginning
  busIO << 0x01 << 0x02 << 0x03 << 0x04 << 0x06 << 0x06;
  /*
  This is the worked out explanation. neb == "not enough bytes".

  01 02 03 | first fetch
  02 03 -- | "no" gets shifted, lastBusAvailable becomes 2.
  02 03 -- | "neb" stays, busIO isn't full. 03 gets a no.
  02 03 04 | fetch is called. lastBusAvailable (2) != bus available (3), so continue with lastBusAvailable as 3. 0x2 and 0x4 are checked
  03 04 -- | busIO is full, so "neb" is removed. lastBusAvailable becomes 2
  04 -- -- | "no" gets shifted, lastBusAvailable becomes 1.
  04 -- -- | "neb" stays, busIO isn't full.
  04 06 06 | fetch is called. lastBusAvailable (1) != bus available (3), so continue with lastBusAvailable as 3.
  06 06 -- | "neb" 04 is removed since the busIO is full
  06 06 -- | yes is found! 0x06 <-> 0x06 is our packet
  */

  // Ensure the bus hasn't fetched a single byte from the serial bus yet, meaning the packetizer calls fetch
  EXPECT_EQ(0, bus.available());
  EXPECT_EQ(0, packetizer.packetLength());

  EXPECT_TRUE(packetizer.hasPacket());
  EXPECT_EQ(2, packetizer.packetLength());

  EXPECT_EQ(2, bus.available());
  EXPECT_EQ(0x06, bus[0]);
  EXPECT_EQ(0x06, bus[1]);
  EXPECT_EQ(-1, bus[2]);

  packetizer.clearPacket();
  EXPECT_FALSE(packetizer.hasPacket());
  EXPECT_EQ(0, packetizer.packetLength());
  EXPECT_EQ(0, bus.available());
  EXPECT_EQ(-1, bus[0]);
}

/**
 * This is the same setup as the test above "test_can_get_packet_even_if_it_is_past_bus_size_if_bytes_are_shifted".
 * The big difference is that fetch must be called 4 times to get our packet whereas we make sure it "times out" after
 * 3 calls.
 */
TEST_F(PacketizerReadBus3Test, cannot_get_packet_if_timeout_is_reached) {
  packetizer.setMaxReadTimeout(200);

  busIO << 0x01 << 0x02 << 0x03 << 0x04 << 0x06 << 0x06;
  When(Method(ArduinoFake(), millis)).Return(
    0,    // Start time ms
    10,   // fetch from bus time
    100,  // Timeout check
    110,  // fetch from bus time
    200   // Timeout check
    );

  // Ensure the bus hasn't fetched a single byte from the serial bus yet, meaning the packetizer calls fetch
  EXPECT_EQ(0, bus.available());
  EXPECT_EQ(0, packetizer.packetLength());

  EXPECT_FALSE(packetizer.hasPacket());
  EXPECT_EQ(0, packetizer.packetLength());

  EXPECT_EQ(2, bus.available());
  EXPECT_EQ(0x04, bus[0]);
  EXPECT_EQ(0x06, bus[1]);
  EXPECT_EQ(-1, bus[2]);
}