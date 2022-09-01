#pragma once

#include <inttypes.h>

class ModbusRTUChecksum {
public:
  void add(uint8_t data);
  operator uint16_t() { return getChecksum(); }
  uint16_t getChecksum() { return checksum; }
private:
  uint16_t checksum = 0xffff;
  static const uint16_t crcTable[256];
};