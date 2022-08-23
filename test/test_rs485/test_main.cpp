#include <gtest/gtest.h>

#include "test_assertable_buffer.h"
#include "test_rs485bus.h"
#include "test_matching_bytes.h"
#include "test_packetizer_read.h"
#include "test_packetizer_write.h"

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