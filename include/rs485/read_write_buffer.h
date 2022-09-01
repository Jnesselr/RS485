#pragma once

#include <stddef.h>

struct ReadWriteBuffer {
  virtual size_t available() = 0;
  virtual int16_t read() = 0;
  virtual void write(uint8_t value) = 0;
};