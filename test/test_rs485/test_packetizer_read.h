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
    When(Method(ArduinoFake(), micros)).AlwaysDo([&]()->unsigned long{
      currentMicros++;
      return currentMicros;
    });
    Spy(Method(protocolSpy, isPacket));

    ArduinoFake().ClearInvocationHistory();
  };

  volatile unsigned long currentMicros = 0;
  AssertableBusIO busIO;
  ProtocolMatchingBytes protocol;
  Mock<ProtocolMatchingBytes> protocolSpy;
};

class PacketizerReadBusTest : public PacketizerReadTest {
protected:
  PacketizerReadBusTest(): PacketizerReadTest(),
    bus(busIO, readEnablePin, writeEnablePin),
    packetizer(bus, protocol) {
      packetizer.setMaxReadTimeout(20);
    }

  RS485Bus<8> bus;
  Packetizer packetizer;
};

class PacketizerReadBus3Test : public PacketizerReadTest {
protected:
  PacketizerReadBus3Test(): PacketizerReadTest(),
    bus(busIO, readEnablePin, writeEnablePin),
    packetizer(bus, protocol) {
      packetizer.setMaxReadTimeout(20);
    }

  RS485Bus<3> bus;
  Packetizer packetizer;
};

class PacketizerReadBusBigTest : public PacketizerReadTest {
protected:
  PacketizerReadBusBigTest(): PacketizerReadTest(),
    bus(busIO, readEnablePin, writeEnablePin),
    packetizer(bus, protocol) {
      packetizer.setMaxReadTimeout(20);
    }

  constexpr static size_t u64Size = sizeof(uint64_t) * 8;  // Needs to match the date type used for recheckBitmap in Packetizer
  constexpr static size_t bufferSize = u64Size + 20;  // uint64_t size + 20 should be good enough for all the tests
  RS485Bus<bufferSize> bus;
  Packetizer packetizer;
};

TEST_F(PacketizerReadBusTest, by_default_no_packets_are_available) {
  EXPECT_FALSE(packetizer.hasPacket());
  EXPECT_EQ(0, packetizer.packetLength());
  Packet packet = packetizer.getPacket();
  EXPECT_EQ(0, packet.startIndex);
  EXPECT_EQ(0, packet.endIndex);
}

TEST_F(PacketizerReadBusTest, can_get_simple_packet) {
  busIO << 0x01 << 0x02 << 0x03 << 0x02 << 0x04;

  // Ensure the bus hasn't fetched a single byte from the serial bus yet, meaning the packetizer calls fetch
  EXPECT_EQ(0, bus.available());
  EXPECT_EQ(0, packetizer.packetLength());

  EXPECT_TRUE(packetizer.hasPacket());
  EXPECT_EQ(3, packetizer.packetLength());
  Packet packet = packetizer.getPacket();
  EXPECT_EQ(0, packet.startIndex);
  EXPECT_EQ(2, packet.endIndex);

  EXPECT_EQ(4, bus.available()); // The first byte isn't part of the packet
  EXPECT_EQ(0x02, bus[0]);
  EXPECT_EQ(0x03, bus[1]);
  EXPECT_EQ(0x02, bus[2]);
  EXPECT_EQ(0x04, bus[3]);
  EXPECT_EQ(-1, bus[4]);

  packetizer.clearPacket();
  EXPECT_FALSE(packetizer.hasPacket());
  EXPECT_EQ(0, packetizer.packetLength());
  packet = packetizer.getPacket();
  EXPECT_EQ(0, packet.startIndex);
  EXPECT_EQ(0, packet.endIndex);
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
  Packet packet = packetizer.getPacket();
  EXPECT_EQ(0, packet.startIndex);
  EXPECT_EQ(0, packet.endIndex);

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
  Packet packet = packetizer.getPacket();
  EXPECT_EQ(0, packet.startIndex);
  EXPECT_EQ(0, packet.endIndex);

  EXPECT_EQ(2, bus.available());
  EXPECT_EQ(0x04, bus[0]);
  EXPECT_EQ(0x06, bus[1]);
  EXPECT_EQ(-1, bus[2]);
}

/**
 * This test is pretty gnarly. The main reason has to do with the flow for the packetizer and how we verify isPacket being called.
 * This also more closely resembles what actual usage would look like and it found some issues that splitting up the test cases did not.
 *
 * Because it's long and hard to read, the verification sections are broken up by comments that start with "--".
 */
TEST_F(PacketizerReadBusBigTest, no_bytes_do_not_get_tested_again) {
  constexpr size_t u64Size = PacketizerReadBusBigTest::u64Size;

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
  Packet packet = packetizer.getPacket();
  EXPECT_EQ(0, packet.startIndex);
  EXPECT_EQ(0, packet.endIndex);

  // -- We want to verify that "isPacket" got called on everything

  for(size_t i = 0; i < u64Size; i++) {
    Verify(Method(protocolSpy, isPacket).Using(_, i, u64Size - 1)).Once();
  }
  VerifyNoOtherInvocations(Method(protocolSpy, isPacket));

  // -- Now we want to call hasPacket again and verify that nothing gets called because no new bytes became available

  // Call our hasPacket method
  EXPECT_FALSE(packetizer.hasPacket());
  EXPECT_EQ(0, packetizer.packetLength());
  EXPECT_EQ(u64Size, bus.available());
  packet = packetizer.getPacket();
  EXPECT_EQ(0, packet.startIndex);
  EXPECT_EQ(0, packet.endIndex);

  // And verify
  VerifyNoOtherInvocations(Method(protocolSpy, isPacket));

  // -- We want to complete a packet that was "not enough bytes" before. Since the size of a long will always be even, we match with size - 2. (size - 1) is currently the last byte on the busIO.

  // Queue up an even number, plus a "no", plus a "not enough bytes"
  uint8_t validPacketByte = u64Size - 2;
  busIO << validPacketByte << (u64Size + 1) << (u64Size + 2);

  // Call our hasPacket method
  EXPECT_TRUE(packetizer.hasPacket());
  EXPECT_EQ(3, packetizer.packetLength());
  EXPECT_EQ(5, bus.available());  // Everything else got cleared out but the packet and our two extra bytes
  packet = packetizer.getPacket();
  EXPECT_EQ(0, packet.startIndex);
  EXPECT_EQ(2, packet.endIndex);

  // And finally, verify
  for(size_t i = 0; i < u64Size; i++) {
    if(i % 2 == 0) {
      Verify(Method(protocolSpy, isPacket).Using(_, i, u64Size + 2)).Once();
    } else {
      Verify(Method(protocolSpy, isPacket).Using(_, i, u64Size + 2)).Never();
    }
  }

  // No bytes after valid packet should have been tested
  VerifyNoOtherInvocations(Method(protocolSpy, isPacket));

  // -- Nothing should have changed, but we want to verify we can still see the packet with nothing having been called

  // Call our hasPacket method
  EXPECT_TRUE(packetizer.hasPacket());
  EXPECT_EQ(3, packetizer.packetLength());
  EXPECT_EQ(5, bus.available());  // Everything else got cleared out but the packet and our two extra bytes
  packet = packetizer.getPacket();
  EXPECT_EQ(0, packet.startIndex);
  EXPECT_EQ(2, packet.endIndex);

  // Verify nothing was called, meaning the packetizer cached the reuslt from last time
  VerifyNoOtherInvocations(Method(protocolSpy, isPacket));

  // -- Clearing the packet will result in (u64Size + 1) and (u64Size + 2) being on the bus. The first is "no" so goes away. We do this step to make sure previous times where things were not re-checked will get re-checked again.
  packetizer.clearPacket();
  EXPECT_EQ(2, bus.available());  // (u64Size + 1) and (u64Size + 2)

  // Call our hasPacket method
  EXPECT_FALSE(packetizer.hasPacket());
  EXPECT_EQ(0, packetizer.packetLength());
  packet = packetizer.getPacket();
  EXPECT_EQ(0, packet.startIndex);
  EXPECT_EQ(0, packet.endIndex);

  Verify(Method(protocolSpy, isPacket).Using(_, 0, 0)).Once();  // (ul64Size + 1) byte -> NO
  Verify(Method(protocolSpy, isPacket).Using(_, 0, 1)).Once();  // (ul64Size + 2) byte -> NOT_ENOUGH_BYTES
  VerifyNoOtherInvocations(Method(protocolSpy, isPacket));

  EXPECT_EQ(1, bus.available());  // Only (u64Size + 2) is on the bus now
  EXPECT_EQ(u64Size + 2, bus[0]);
  EXPECT_EQ(-1, bus[1]);
}

TEST_F(PacketizerReadBusBigTest, no_bytes_past_our_limit_get_rechecked) {
  constexpr size_t u64Size = PacketizerReadBusBigTest::u64Size;

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
  Packet packet = packetizer.getPacket();
  EXPECT_EQ(0, packet.startIndex);
  EXPECT_EQ(0, packet.endIndex);

  // -- We want to verify that "isPacket" got called on everything
  for(size_t i = 0; i < bus.bufferSize(); i++) {
    if(i <= u64Size) {
      Verify(Method(protocolSpy, isPacket).Using(_, i, u64Size)).Once();
    } else {
      Verify(Method(protocolSpy, isPacket).Using(_, i, u64Size)).Never();
    }
  }
  VerifyNoOtherInvocations(Method(protocolSpy, isPacket));

  // -- Now we want to call hasPacket again and verify that the NO past u64Size gets called on again.

  // Queue a byte that will get checked regardless. Otherwise hasPacket will exit without rechecking anything
  busIO << 255;

  // Call our hasPacket method
  EXPECT_FALSE(packetizer.hasPacket());
  EXPECT_EQ(0, packetizer.packetLength());
  EXPECT_EQ(u64Size + 2, bus.available());
  packet = packetizer.getPacket();
  EXPECT_EQ(0, packet.startIndex);
  EXPECT_EQ(0, packet.endIndex);

  // Verify that only bytes at (0), (u64Size), and (u64Size + 1) were checked.
  // (u64Size + 1) wasn't ever checked, (0) is our "not enough bytes", and (u64Size) is past where we're able to check it.
  for(size_t i = 0; i < bus.bufferSize(); i++) {
    if(i == 0 || i == u64Size || i == (u64Size + 1)) {
      Verify(Method(protocolSpy, isPacket).Using(_, i, u64Size + 1)).Once();
    } else {
      Verify(Method(protocolSpy, isPacket).Using(_, i, u64Size + 1)).Never();
    }
  }
  VerifyNoOtherInvocations(Method(protocolSpy, isPacket));
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

  ASSERT_TRUE(packetizer.hasPacket());
  EXPECT_EQ(2, packetizer.packetLength());
  Packet packet = packetizer.getPacket();
  EXPECT_EQ(0, packet.startIndex);
  EXPECT_EQ(1, packet.endIndex);

  EXPECT_EQ(2, bus.available());
  EXPECT_EQ(0x06, bus[0]);
  EXPECT_EQ(0x06, bus[1]);
  EXPECT_EQ(-1, bus[2]);

  packetizer.clearPacket();
  ASSERT_FALSE(packetizer.hasPacket());
  EXPECT_EQ(0, packetizer.packetLength());
  packet = packetizer.getPacket();
  EXPECT_EQ(0, packet.startIndex);
  EXPECT_EQ(0, packet.endIndex);
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
  When(Method(ArduinoFake(), micros)).Return(
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
  Packet packet = packetizer.getPacket();
  EXPECT_EQ(0, packet.startIndex);
  EXPECT_EQ(0, packet.endIndex);

  EXPECT_EQ(2, bus.available());
  EXPECT_EQ(0x04, bus[0]);
  EXPECT_EQ(0x06, bus[1]);
  EXPECT_EQ(-1, bus[2]);
}