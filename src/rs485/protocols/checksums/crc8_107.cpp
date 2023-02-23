#include "rs485/protocols/checksums/crc8_107.h"

#include <cstddef>

void CRC8_107::add(uint8_t data) {
  crc ^= (data << 8);
  for (size_t bit_n = 0; bit_n < 8; bit_n++) {
      if (crc & 0x8000)
          crc ^= (0x1070 << 3);
      crc <<= 1;
  }
}

uint8_t CRC8_107::getChecksum() {
  return (uint8_t)(crc >> 8);
}