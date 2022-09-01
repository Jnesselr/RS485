#pragma once

#include "rs485/rs485bus_base.h"

template<size_t BufferSize>
class RS485Bus: public RS485BusBase {
public:
  RS485Bus(ReadWriteBuffer& buffer, uint8_t readEnablePin, uint8_t writeEnablePin);

  size_t bufferSize() const;

protected:
  virtual void setByte(size_t bufferIndex, uint8_t value);
  virtual uint8_t getByte(size_t bufferIndex) const;

private:
  uint8_t readBuffer[BufferSize];
};

template<size_t BufferSize>
RS485Bus<BufferSize>::RS485Bus(ReadWriteBuffer& buffer, uint8_t readEnablePin, uint8_t writeEnablePin) :
  RS485BusBase(buffer, readEnablePin, writeEnablePin)
  {
    readBufferSize = BufferSize;
  }

template<size_t BufferSize>
size_t RS485Bus<BufferSize>::bufferSize() const {
  return BufferSize;
}

template<size_t BufferSize>
void RS485Bus<BufferSize>::setByte(size_t bufferIndex, uint8_t value) {
  readBuffer[bufferIndex] = value;
}

template<size_t BufferSize>
uint8_t RS485Bus<BufferSize>::getByte(size_t bufferIndex) const {
  return readBuffer[bufferIndex];
}