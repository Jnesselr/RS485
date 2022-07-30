#ifdef UNIT_TEST
#include <unity.h>

#include "assertable_buffer.h"
#include "rs485bus.hpp"
#include "packetizer.h"

namespace PacketizerTest {
  const uint8_t readEnablePin = 13;
  const uint8_t writeEnablePin = 14;
  void setUpArduinoFake() {
    When(Method(ArduinoFake(), pinMode)).AlwaysReturn();
    When(Method(ArduinoFake(), digitalWrite)).AlwaysReturn();
  }

  void test_by_default_no_packets_are_available() {
    AssertableBuffer buffer;
    setUpArduinoFake();
    RS485Bus<8> bus(buffer, readEnablePin, writeEnablePin);
    Packetizer packetizer(bus);

    TEST_ASSERT_FALSE(packetizer.hasPacket());
  }

  void run_tests() {
    RUN_TEST(test_by_default_no_packets_are_available);
  }
}

#endif