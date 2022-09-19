#pragma once

#include "../bus_io.h"
#include <arduino/HardwareSerial.h>

class HardwareSerialBusIO : public BusIO {
public:
  HardwareSerialBusIO(HardwareSerial* serial) : serial(serial) {};
  virtual size_t available();
  virtual int16_t read();
  virtual void write(uint8_t value);
protected:
  HardwareSerial* serial;
};