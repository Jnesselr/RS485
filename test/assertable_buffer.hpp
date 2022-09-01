#pragma once

#include <inttypes.h>

#include "rs485/read_write_buffer.h"
#include "fakeit/fakeit.hpp"

#include <initializer_list>


class AssertableBuffer: public ReadWriteBuffer {
public:
  AssertableBuffer();

  AssertableBuffer& readable(uint8_t readable);
  AssertableBuffer& operator<<(uint8_t readable);
  template<size_t bufferSize> AssertableBuffer& readable(std::array<uint8_t, bufferSize> readable);
  int16_t operator[](size_t index);
  size_t written();

  // From ReadWriteBuffer
  virtual size_t available();
  virtual int16_t read();
  virtual void write(uint8_t value);

  fakeit::Mock<AssertableBuffer> spy();

private:
  static const size_t BUFFER_SIZE = 128;
  unsigned char buffer[AssertableBuffer::BUFFER_SIZE];
  size_t head = 0;
  size_t tail = 0;

  // We're not doing anything to prevent overflow, but it's just for testing
  uint8_t writtenBuffer[1024];
  size_t writtenHead = 0;
  size_t writtenTail = 0;
};

template<size_t bufferSize>
AssertableBuffer& AssertableBuffer::readable(std::array<uint8_t, bufferSize> readable) {
  for(auto it = std::begin(readable); it != std::end(readable); ++it) {
    this->readable(*it);
  }

  return (*this);
}