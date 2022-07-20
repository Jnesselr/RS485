#include "test_buffer.h"

TestBuffer::TestBuffer() {

}

int TestBuffer::available() {
  return ((tail - head) + BUFFER_SIZE) % BUFFER_SIZE;
}

int TestBuffer::read() {
  if(head == tail) {
    return -1; // Nothing to read
  }
  int result = buffer[head];
  head = (head + 1) % BUFFER_SIZE;

  return result;
}

void TestBuffer::readable(const int& readable) {
  buffer[tail] = readable;
  tail = (tail + 1) % BUFFER_SIZE;
}

TestBuffer& TestBuffer::operator<<(const int& readable) {
  this->readable(readable);
  return *this;
}

int TestBuffer::operator[](const int& index) {
  int bufferPosition = (head + index) % BUFFER_SIZE;
  return buffer[bufferPosition];
}

int TestBuffer::written() {
  if(writtenHead == writtenTail) {
    return -1;
  }
  return writtenBuffer[writtenHead++];
}

void TestBuffer::write(const unsigned char& value) {
  writtenBuffer[writtenTail] = value;
  writtenTail++;
}