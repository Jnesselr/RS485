#pragma once

#include "../assertable_bus_io.hpp"
#include "../matching_bytes.h"
#include "../fixtures.h"

#include <gtest/gtest.h>
#include "fakeit/fakeit.hpp"

#include "rs485/rs485bus.hpp"
#include "rs485/packetizer.h"

using namespace fakeit;

// Notes:
// Has Packet inner loop kinda doesn't matter anymore. It can be removed
// We should not clear out the packet in hasPacketNow if our post filter isn't good. We should just skip over it. There's two reasons for this. The first is that if we find a packet in the middle of another packet, it may not pass our post filter, but it's fine it wasn't our real packet anyway. Which does mean we need to call post filter again, unfortunately, in hasPacket. The second reason is that or really the second thing we need to handle is when we have a packet filtered out and our buffer is full. We need to eat a byte, at least.


/**
 * TODO:
 * - Packet filtering
 *   / Update tests to make sure we don't clear the packet automatically
 *   / Add a test to make sure we do eat a byte if our buffer is full (sorta not needed)
 *   / Add a test to clear it if it's not our packet and it starts at the first byte
 *   - Get that code to work
 * - Has packet with delay
 *   / Not a test: Verify in our spy for hasPacketNow, we can actually update startIndex and endIndex as a side effect. They might need to be protected.
 * 
 *   / Verify our maxReadTimeout does in fact time out
 *   / Verify we're calling hasPacketNow in a very simple test that does no delay if hasPacketNow returns false but no delay is set
 *   / Verify we're calling hasPacketNow in a very simple test that does no delay if hasPacketNow returns true but no delay is set
 *   / Verify calling hasPacket has no delay if hasPacketNow is false and there is a delay
 *   / Verify we're waiting our false packet verification delay IF we have a yes from hasPacketNow to verify the packet
 *   / Verify we're not waiting for a false packet if our packet starts on the first byte
 *   / Do we want to wait the false packet timeout EVERY time there's an updated packet? I would think maybe so.
 */

// class PacketizerReadWithFetchTest;
// typedef void (PacketizerReadWithFetchTest::*HasPacketNowCalled)();

class FetchingPacketizer : public Packetizer {
public:
  explicit FetchingPacketizer(
    std::function<void(void)> callback,
    RS485BusBase& bus,
    const Protocol& protocol): Packetizer(bus, protocol),
  callback(callback)
  {}

  // fetchFromBus is only called once per loop in hasPacket which makes it ideal to also increment the micros() count and read new bytes in.
  virtual size_t fetchFromBus() {
    size_t result = Packetizer::hasPacketNow();
    callback();

    return result;
  }
private:
  std::function<void(void)> callback;
};

class PacketizerReadWithFetchTest: public PrepBus {
protected:
  PacketizerReadWithFetchTest(): PrepBus(),
    bus(busIO, readEnablePin, writeEnablePin),
    packetizer(std::bind(&PacketizerReadWithFetchTest::fetchFromBusCallback, this), bus, protocol),
    packetizerSpy(packetizer)
     {};

  void SetUp() {
    When(Method(ArduinoFake(), micros)).AlwaysDo([&]()->unsigned long{
      return currentMicros;
    });

    Spy(Method(packetizerSpy, hasPacketNow));

    ArduinoFake().ClearInvocationHistory();

    memset(microsToReadData, 0, sizeof(TimeMicroseconds_t) * sizeof(microsToReadData));
    memset(newReadableCounts, 0, sizeof(size_t) * sizeof(newReadableCounts));
    memset(buffer, 0, sizeof(uint8_t) * sizeof(buffer));
  };

  void expectNoPacket() {
    Packet packet = packetizer.getPacket();
    EXPECT_EQ(0, packet.startIndex);
    EXPECT_EQ(0, packet.endIndex);
  }

  void expectPacket(size_t startIndex, size_t endIndex) {
    Packet packet = packetizer.getPacket();
    EXPECT_EQ(startIndex, packet.startIndex);
    EXPECT_EQ(endIndex, packet.endIndex);
  }

  template<size_t bufferSize> 
  void newDataAtTime(TimeMicroseconds_t time, std::array<uint8_t, bufferSize> readable) {
    microsToReadData[microsWriteIndex] = time;
    newReadableCounts[microsWriteIndex] = bufferSize;

    for (size_t i = 0; i < bufferSize; i++)
    {
      buffer[writeBufferIndex] = readable[i];
      writeBufferIndex++;
    }

    microsWriteIndex++;
  }

  volatile unsigned long currentMicros = 0;
  AssertableBusIO busIO;
  RS485Bus<8> bus;
  ProtocolMatchingBytes protocol;
  FetchingPacketizer packetizer;
  Mock<FetchingPacketizer> packetizerSpy;

private:
  void fetchFromBusCallback() {
    currentMicros++;
    std::cout << std::endl;
    std::cout << "fetch from bus callback at " << currentMicros << std::endl;

    std::cout << "micros to read: " << microsToReadData[microsReadIndex] << std::endl;

    if(currentMicros != microsToReadData[microsReadIndex]) {
      return;
    }

    size_t bytesToReadIn = newReadableCounts[microsReadIndex];
    std::cout << "bytes to read in (" << bytesToReadIn << "): ";
    for (size_t i = 0; i < bytesToReadIn; i++)
    {
      std::cout << buffer[readBufferIndex] << " ";
      busIO << buffer[readBufferIndex];
      readBufferIndex++;
    }
    std::cout << std::endl;
    bus.fetch();

    microsReadIndex++;
  }

  size_t microsReadIndex = 0;  // Current location we check micros against to see if we need to read in data.
  size_t microsWriteIndex = 0;  // Next location to write into microsToReadableIndex.
  TimeMicroseconds_t microsToReadData[100];  // each index is the next time to read in more bytes at the same newReadableCounts index.
  size_t newReadableCounts[100];  // After each hasPacketNow, if we're at the right micros the current index is how many new bytes to read from our buffer into busIO.
  size_t writeBufferIndex = 0;  // Next location to write data into our buffer
  size_t readBufferIndex = 0;  // Next location to read data into from buffer
  unsigned char buffer[1024];  // Our buffer to read X bytes from
};

TEST_F(PacketizerReadWithFetchTest, max_read_timeout_does_not_matter_if_no_new_bytes_come_in) {
    packetizer.setMaxReadTimeout(10);

    ASSERT_FALSE(packetizer.hasPacket());
    expectNoPacket();

    ASSERT_EQ(1, currentMicros);  // Only 1 microsecond has passed

    Verify(Method(packetizerSpy, hasPacketNow)).Once();
}

TEST_F(PacketizerReadWithFetchTest, max_read_timeout_is_hit_if_new_bytes_keep_coming_in) {
    packetizer.setMaxReadTimeout(10);

    for (size_t i = 0; i < 20; i++)  // 20 here, but we should only see the first 10 and not care past that
    {
      uint8_t noByte = (2 * i) + 1;  // A no byte here just means our bus keeps getting cleared too.
      newDataAtTime<1>(i + 1, { noByte });
    }

    ASSERT_FALSE(packetizer.hasPacket());
    expectNoPacket();

    ASSERT_EQ(11, currentMicros);

    Verify(Method(packetizerSpy, hasPacketNow)).Exactly(11_Times);
}

TEST_F(PacketizerReadWithFetchTest, simple_has_packet_with_no_delay) {
    packetizer.setMaxReadTimeout(100);

    busIO << 0x02 << 0x04 << 0x04;
    bus.fetch();

    newDataAtTime<1>(5, {0x02});  // This should not matter as there's no packet delay

    ASSERT_TRUE(packetizer.hasPacket());
    expectPacket(1, 2);

    ASSERT_EQ(1, currentMicros);  // Only 1 microsecond has passed

    Verify(Method(packetizerSpy, hasPacketNow)).Once();
}

TEST_F(PacketizerReadWithFetchTest, false_read_timeout_does_not_matter_if_there_is_no_packet) {
    packetizer.setMaxReadTimeout(100);
    packetizer.setFalsePacketVerificationTimeout(5);

    ASSERT_FALSE(packetizer.hasPacket());
    expectNoPacket();

    ASSERT_EQ(1, currentMicros);  // Only 1 microsecond has passed

    Verify(Method(packetizerSpy, hasPacketNow)).Once();
}

TEST_F(PacketizerReadWithFetchTest, has_packet_does_not_delay_if_packet_starts_on_first_byte) {
    packetizer.setMaxReadTimeout(100);
    packetizer.setFalsePacketVerificationTimeout(5);

    busIO << 0x02 << 0x01 << 0x02;
    bus.fetch();

    newDataAtTime<1>(5, {0x01});  // This should not matter as our packet is read and starts on the first byte

    ASSERT_TRUE(packetizer.hasPacket());
    expectPacket(0, 2);

    ASSERT_EQ(0, currentMicros);  // No time has passed, we didn't bother fetching more bytes from the bus.

    Verify(Method(packetizerSpy, hasPacketNow)).Once();
}

TEST_F(PacketizerReadWithFetchTest, simple_has_packet_with_false_verification_delay) {
    packetizer.setMaxReadTimeout(100);
    packetizer.setFalsePacketVerificationTimeout(5);

    busIO << 0x02 << 0x04 << 0x04;
    bus.fetch();

    newDataAtTime<1>(5, {0x02});  // This is read in right on time

    ASSERT_TRUE(packetizer.hasPacket());
    expectPacket(0, 3);

    ASSERT_EQ(5, currentMicros);

    Verify(Method(packetizerSpy, hasPacketNow)).Exactly(2_Times);
}

TEST_F(PacketizerReadWithFetchTest, false_verification_delay_can_time_out) {
    packetizer.setMaxReadTimeout(100);
    packetizer.setFalsePacketVerificationTimeout(5);

    busIO << 0x02 << 0x04 << 0x04;
    bus.fetch();

    newDataAtTime<1>(10, {0x02});  // This would be read in after our verification timeout

    ASSERT_TRUE(packetizer.hasPacket());
    expectPacket(1, 2);

    ASSERT_EQ(6, currentMicros);

    Verify(Method(packetizerSpy, hasPacketNow)).Once();
}

TEST_F(PacketizerReadWithFetchTest, verification_delay_happens_each_time) {
    packetizer.setMaxReadTimeout(100);
    packetizer.setFalsePacketVerificationTimeout(5);

    busIO << 0x02 << 0x04 << 0x06 << 0x06;
    bus.fetch();

    newDataAtTime<1>(5, {0x04});
    newDataAtTime<1>(10, {0x02});

    ASSERT_TRUE(packetizer.hasPacket());
    expectPacket(0, 5);

    ASSERT_EQ(10, currentMicros);

    Verify(Method(packetizerSpy, hasPacketNow)).Exactly(3_Times);
}



TEST_F(PacketizerReadWithFetchTest, verification_waits_until_timeout_if_packet_does_not_start_at_bus_start) {
    packetizer.setMaxReadTimeout(100);
    packetizer.setFalsePacketVerificationTimeout(5);

    busIO << 0x02 << 0x04 << 0x06 << 0x06;
    bus.fetch();

    newDataAtTime<1>(5, {0x04});

    ASSERT_TRUE(packetizer.hasPacket());
    expectPacket(1, 4);

    ASSERT_EQ(11, currentMicros);

    Verify(Method(packetizerSpy, hasPacketNow)).Exactly(2_Times);
}