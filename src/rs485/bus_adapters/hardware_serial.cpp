#include "rs485/bus_adapters/hardware_serial.h"

size_t HardwareSerialBusIO::available() {
  return serial->available();
}

int16_t HardwareSerialBusIO::read() {
  return serial->read();
}

void HardwareSerialBusIO::write(uint8_t value) {
  serial->write(value);
}