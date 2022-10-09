#pragma once

#include "../bus_io.h"
#ifdef UNIT_TEST
#include <arduino/HardwareSerial.h>
#else
#include <HardwareSerial.h>
#endif

class HardwareSerialBusIO : public BusIO {
public:
  HardwareSerialBusIO(HardwareSerial* serial) : serial(serial) {};
  virtual size_t available();
  virtual int16_t read();
  virtual void write(uint8_t value);
protected:
  HardwareSerial* serial;
};