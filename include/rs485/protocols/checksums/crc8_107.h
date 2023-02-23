#pragma once

#include <inttypes.h>

// Polynomial: X^8 + X^2 + X + 1
class CRC8_107 {
public:
  void add(uint8_t data);
  operator uint8_t() { return getChecksum(); }
  uint8_t getChecksum();
private:
  uint32_t crc = 0x0;
};