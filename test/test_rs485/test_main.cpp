#include <gtest/gtest.h>

// Test Helpers
#include "test_assertable_bus_io.h"
#include "test_matching_bytes.h"

// Core code
#include "test_rs485bus.h"
#include "test_packetizer_read.h"
#include "test_packetizer_read_with_fetch.h"
#include "test_packetizer_write.h"
#include "test_packetizer_filter.h"

// Filters
#include "filters/test_filter_by_value.h"
#include "filters/test_combo_filter.h"

// Protocols
#include "protocols/test_photon.h"

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    if (RUN_ALL_TESTS())
    ;

    return 0;
}