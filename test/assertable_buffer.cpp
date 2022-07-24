#include "assertable_buffer.h"

AssertableBuffer::AssertableBuffer() {

}

int AssertableBuffer::available() {
  return ((tail - head) + BUFFER_SIZE) % BUFFER_SIZE;
}

int AssertableBuffer::read() {
  if(head == tail) {
    return -1; // Nothing to read
  }
  int result = buffer[head];
  head = (head + 1) % BUFFER_SIZE;

  return result;
}

void AssertableBuffer::readable(const int& readable) {
  buffer[tail] = readable;
  tail = (tail + 1) % BUFFER_SIZE;
}

AssertableBuffer& AssertableBuffer::operator<<(const int& readable) {
  this->readable(readable);
  return *this;
}

int AssertableBuffer::operator[](const int& index) {
  if (index >= available()) {
    return -1;
  }
  int bufferPosition = (head + index) % BUFFER_SIZE;
  return buffer[bufferPosition];
}

int AssertableBuffer::written() {
  if(writtenHead == writtenTail) {
    return -1;
  }
  return writtenBuffer[writtenHead++];
}

void AssertableBuffer::write(const unsigned char& value) {
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