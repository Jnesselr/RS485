#include <gtest/gtest.h>

// Test Helpers
#include "test_assertable_bus_io.h"
#include "test_matching_bytes.h"

// Core code
#include "test_rs485bus.h"
#include "test_packetizer_read.h"
// #include "test_packetizer_write.h"
// #include "test_packetizer_filter.h"

// Filters
// #include "filters/test_filter_by_value.h"
// #include "filters/test_combo_filter.h"

// Protocols
// #include "protocols/test_photon.h"

#if defined(ARDUINO)
#include <Arduino.h>

void setup()
{
    // should be the same value as for the `test_speed` option in "platformio.ini"
    // default value is test_speed=115200
    Serial.begin(115200);

    ::testing::InitGoogleTest();
}

void loop()
{
  // Run tests
  if (RUN_ALL_TESTS())
  ;

  // sleep for 1 sec
  delay(1000);
}

#else
int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    if (RUN_ALL_TESTS())
    ;

    return 0;
}
#endif