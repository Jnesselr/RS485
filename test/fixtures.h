#pragma once

#include <gtest/gtest.h>

// We need our arduino fake up and running before we get to the RS485Bus initialization in the RS485BusTest constructor
class PrepBus : public ::testing::Test {
public:
  PrepBus();

  const uint8_t readEnablePin = 13;
  const uint8_t writeEnablePin = 14;
};