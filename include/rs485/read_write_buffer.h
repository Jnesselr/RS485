#pragma once

struct ReadWriteBuffer {
  virtual int available() = 0;
  virtual int read() = 0;
  virtual void write(const unsigned char& value) = 0;
};