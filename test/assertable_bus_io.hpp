#pragma once

#include <inttypes.h>

#include "rs485/bus_io.h"
#include "fakeit/fakeit.hpp"

#include <initializer_list>


class AssertableBusIO: public BusIO {
public:
  AssertableBusIO();

  AssertableBusIO& readable(uint8_t readable);
  AssertableBusIO& operator<<(uint8_t readable);
  template<size_t bufferSize> AssertableBusIO& readable(std::array<uint8_t, bufferSize> readable);
  int16_t operator[](size_t index);
  size_t written();

  // From BusIO
  virtual size_t available();
  virtual int16_t read();
  virtual void write(uint8_t value);

  fakeit::Mock<AssertableBusIO> spy();

private:
  static const size_t BUFFER_SIZE = 128;
  unsigned char buffer[AssertableBusIO::BUFFER_SIZE];
  size_t head = 0;
  size_t tail = 0;

  // We're not doing anything to prevent overflow, but it's just for testing
  uint8_t writtenBuffer[1024];
  size_t writtenHead = 0;
  size_t writtenTail = 0;
};

template<size_t bufferSize>
AssertableBusIO& AssertableBusIO::readable(std::array<uint8_t, bufferSize> readable) {
  for(auto it = std::begin(readable); it != std::end(readable); ++it) {
    this->readable(*it);
  }

  return (*this);
}

AssertableBusIO::AssertableBusIO() {

}

size_t AssertableBusIO::available() {
  return ((tail - head) + BUFFER_SIZE) % BUFFER_SIZE;
}

int16_t AssertableBusIO::read() {
  if(head == tail) {
    return -1; // Nothing to read
  }
  int16_t result = buffer[head];
  head = (head + 1) % BUFFER_SIZE;

  return result;
}

AssertableBusIO& AssertableBusIO::readable(uint8_t readable) {
  buffer[tail] = readable;
  tail = (tail + 1) % BUFFER_SIZE;

  return *this;
}

AssertableBusIO& AssertableBusIO::operator<<(uint8_t readable) {
  return this->readable(readable);
}

int16_t AssertableBusIO::operator[](size_t index) {
  if (index >= available()) {
    return -1;
  }
  int bufferPosition = (head + index) % BUFFER_SIZE;
  return buffer[bufferPosition];
}

size_t AssertableBusIO::written() {
  if(writtenHead == writtenTail) {
    return -1;
  }
  return writtenBuffer[writtenHead++];
}

void AssertableBusIO::write(uint8_t value) {
  writtenBuffer[writtenTail] = value;
  writtenTail++;
}

fakeit::Mock<AssertableBusIO> AssertableBusIO::spy() {
  fakeit::Mock<AssertableBusIO> spy(*this);

  fakeit::Spy(Method(spy, write));
  fakeit::Spy(Method(spy, read));
  fakeit::Spy(Method(spy, available));

  return spy;
}