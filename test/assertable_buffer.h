#pragma once

#include "rs485/read_write_buffer.h"
#include "fakeit/fakeit.hpp"


class AssertableBuffer: public ReadWriteBuffer {
public:
  AssertableBuffer();

  void readable(const int& readable);
  AssertableBuffer& operator<<(const int& readable);
  int operator[](const int& index);
  int written();

  // From ReadWriteBuffer
  virtual int available();
  virtual int read();
  virtual void write(const unsigned char& value);

  fakeit::Mock<AssertableBuffer> spy();

private:
  static const int BUFFER_SIZE = 128;
  unsigned char buffer[AssertableBuffer::BUFFER_SIZE];
  int head = 0;
  int tail = 0;

  // We're not doing anything to prevent overflow, but it's just for testing
  unsigned char writtenBuffer[1024];
  int writtenHead = 0;
  int writtenTail = 0;
};