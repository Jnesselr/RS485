#pragma once

#include <stddef.h>
#include <inttypes.h>

/**
 * This is a wrapper around serial-like classes. The RS485Bus class takes a BusIO instance in its constructor.
 */
class BusIO {
public:
  virtual ~BusIO() {}  // Virtual destructor
  virtual size_t available() = 0;  // How many bytes are available to be read in. If N bytes are available, read should be able to be called N times.
  virtual int16_t read() = 0;  // Read one byte from the buffer and return it. Returns -1 if no bytes are available.
  virtual void write(uint8_t value) = 0;  // Write one byte to the bus. Adjusting write enable pins are not the responsibility of this class.
};