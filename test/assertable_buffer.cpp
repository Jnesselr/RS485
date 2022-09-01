#include "assertable_buffer.hpp"

AssertableBuffer::AssertableBuffer() {

}

size_t AssertableBuffer::available() {
  return ((tail - head) + BUFFER_SIZE) % BUFFER_SIZE;
}

int16_t AssertableBuffer::read() {
  if(head == tail) {
    return -1; // Nothing to read
  }
  int16_t result = buffer[head];
  head = (head + 1) % BUFFER_SIZE;

  return result;
}

AssertableBuffer& AssertableBuffer::readable(uint8_t readable) {
  buffer[tail] = readable;
  tail = (tail + 1) % BUFFER_SIZE;

  return *this;
}

AssertableBuffer& AssertableBuffer::operator<<(uint8_t readable) {
  return this->readable(readable);
}

int16_t AssertableBuffer::operator[](size_t index) {
  if (index >= available()) {
    return -1;
  }
  int bufferPosition = (head + index) % BUFFER_SIZE;
  return buffer[bufferPosition];
}

size_t AssertableBuffer::written() {
  if(writtenHead == writtenTail) {
    return -1;
  }
  return writtenBuffer[writtenHead++];
}

void AssertableBuffer::write(uint8_t value) {
  writtenBuffer[writtenTail] = value;
  writtenTail++;
}

fakeit::Mock<AssertableBuffer> AssertableBuffer::spy() {
  fakeit::Mock<AssertableBuffer> spy(*this);

  fakeit::Spy(Method(spy, write));
  fakeit::Spy(Method(spy, read));
  fakeit::Spy(Method(spy, available));

  return spy;
}