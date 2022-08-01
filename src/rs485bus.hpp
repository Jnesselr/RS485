#pragma once

#include "rs485bus_base.h"

template<int BufferSize>
class RS485Bus: public RS485BusBase {
public:
  RS485Bus(ReadWriteBuffer& buffer, int readEnablePin, int writeEnablePin);

  int bufferSize() const;

protected:
  virtual void setByte(const int& bufferIndex, const unsigned char value);
  virtual const unsigned char getByte(const int& bufferIndex) const;

private:
  unsigned char readBuffer[BufferSize];
};

template<int BufferSize>
RS485Bus<BufferSize>::RS485Bus(ReadWriteBuffer& buffer, int readEnablePin, int writeEnablePin) :
  RS485BusBase(buffer, readEnablePin, writeEnablePin)
  {
    readBufferSize = BufferSize;
  }

template<int BufferSize>
int RS485Bus<BufferSize>::bufferSize() const {
  return BufferSize;
}

template<int BufferSize>
void RS485Bus<BufferSize>::setByte(const int& bufferIndex, const unsigned char value) {
  readBuffer[index] = value;
}

template<int BufferSize>
const unsigned char RS485Bus<BufferSize>::getByte(const int& bufferIndex) const {
  return readBuffer[index];
}