#pragma once

#include "read_write_buffer.h"


class TestBuffer: public ReadWriteBuffer {
public:
  TestBuffer();

  void readable(const int& readable);
  TestBuffer& operator<<(const int& readable);
  int operator[](const int& index);
  int written();

  // From ReadWriteBuffer
  virtual int available();
  virtual int read();
  virtual void write(const unsigned char& value);

private:
  static const int BUFFER_SIZE = 128;
  unsigned char buffer[TestBuffer::BUFFER_SIZE];
  int head = 0;
  int tail = 0;

  // We're not doing anything to prevent overflow, but it's just for testing
  unsigned char writtenBuffer[1024];
  int writtenHead = 0;
  int writtenTail = 0;
};