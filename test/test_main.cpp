#include <Arduino.h>
#include <cstdlib>
#include <unity.h>

#ifdef UNIT_TEST

#include "test_assertable_buffer.h"
#include "test_rs485bus.h"

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

    RUN_TEST_GROUP(AssertableBufferTest);
    RUN_TEST_GROUP(RS485BusTest);

    UNITY_END();

    return 0;
}

#endif