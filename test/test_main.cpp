#include <Arduino.h>
#include <cstdlib>
#include <unity.h>

#ifdef UNIT_TEST

#include "test_assertable_buffer.h"
#include "test_rs485bus.h"
#include "test_packetizer_read.h"
#include "test_matching_bytes.h"

#define RUN_TEST_GROUP(TEST) \
    if (!std::getenv("TEST_GROUP") || (strcmp(#TEST, std::getenv("TEST_GROUP")) == 0)) { \
        TEST::run_tests(); \
    }

void setUp(void)
{
    ArduinoFakeReset();
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();

    RUN_TEST_GROUP(AssertableBufferTest); // Tests a class used in tests for future tests
    RUN_TEST_GROUP(RS485BusTest);
    RUN_TEST_GROUP(PacketMatchingBytesTest); // Tests a class used in tests for future tests
    RUN_TEST_GROUP(PacketizerReadTest);

    UNITY_END();

    return 0;
}

#endif