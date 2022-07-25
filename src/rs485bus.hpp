#pragma once

#include "Arduino.h"
#include "read_write_buffer.h"

enum class WriteStatus {
  OK,                       // Read the written byte right back.
  UNEXPECTED_EXTRA_BYTES,   // Read extra bytes back but eventually found the one we wrote. See notes below.
  NO_READ_TIMEOUT,          // No bytes read at all, timed out waiting
  FAILED_READ_BACK,         // Read some bytes but failed to see our byte
  READ_BUFFER_FULL          // Our internal buffer is full. Byte was still written, but we won't check it. See notes below.
};

/*
Notes on WriteStatus:
- UNEXPECTED_EXTRA_BYTES: The byte we read back might be one we've sent or it might be from another source. This layer of the library leaves it up to the caller to decide what to do. It may be that this is the first byte of a packet that you're sending and everything up to that byte is valid, so no collision has yet occurred. If it does belong to another sender, then the bus essentially ate it which is less ideal.
- READ_BUFFER_FULL: In the event we read back a value different from what we write, we put that value in an internal buffer for the caller to access later. It is the caller's responsibility to make sure this buffer is not full before they start writing, as well as making sure the ReadWriteBuffer has no bytes available by calling fetch(). However, if the reader keeps getting data available to it within the retries/timeout it will also fill up the buffer even if it was empty to start.
*/

template<int BufferSize>
class RS485Bus {
public:
  RS485Bus(ReadWriteBuffer& buffer, int readEnablePin, int writeEnablePin);

  WriteStatus write(const unsigned char& value);

  int bufferSize();
  int available();
  bool isBufferFull();
  int fetch();  // Returns how many bytes were fetched

  int operator[](const int& index);

  void setReadBackDelayMs(int milliseconds);
  void setReadBackRetries(int retryCount);

private:
  void putByteInBuffer(const unsigned char& value);

  ReadWriteBuffer& buffer;
  int readEnablePin;
  int writeEnablePin;

  unsigned char readBuffer[BufferSize];
  int head = 0;
  int tail = 0;
  bool full = false;

  int readBackRetryMilliseconds = 0;
  int readBackRetryCount = 0;
};

template<int BufferSize>
RS485Bus<BufferSize>::RS485Bus(ReadWriteBuffer& buffer, int readEnablePin, int writeEnablePin) :
  buffer(buffer),
  readEnablePin(readEnablePin),
  writeEnablePin(writeEnablePin) {
    pinMode(readEnablePin, OUTPUT);
    pinMode(writeEnablePin, OUTPUT);
    digitalWrite(readEnablePin, LOW);
    digitalWrite(writeEnablePin, LOW);
}

template<int BufferSize>
int RS485Bus<BufferSize>::bufferSize() {
  return BufferSize;
}

template<int BufferSize>
WriteStatus RS485Bus<BufferSize>::write(const unsigned char& writeValue) {
  digitalWrite(writeEnablePin, HIGH);

  buffer.write(writeValue);

  digitalWrite(writeEnablePin, LOW);

  bool readUnexpectedBytes = false;

  while(true) {
    bool bytesAvailable = (buffer.available() > 0);
    if(! bytesAvailable) {
      for(int i=0; i < readBackRetryCount; i++) {
        delay(readBackRetryMilliseconds);

        bytesAvailable |= (buffer.available() > 0);
        if(bytesAvailable) {
          break;
        }
      }
    }

    if(! bytesAvailable) {
      if(readUnexpectedBytes) {
        return WriteStatus::FAILED_READ_BACK;
      } else {
        return WriteStatus::NO_READ_TIMEOUT;
      }
    }
    
    if(full) {
      // Refuse to even read the byte, because if it's not the one we expect, we can't put it in the buffer.
      return WriteStatus::READ_BUFFER_FULL;
    }

    int readValue = buffer.read();
    if(readValue == writeValue) {
      if(readUnexpectedBytes) {
        return WriteStatus::UNEXPECTED_EXTRA_BYTES;
      }
      return WriteStatus::OK;
    } else {
      readUnexpectedBytes = true;
      putByteInBuffer(readValue);
    }
  }
}

template<int BufferSize>
int RS485Bus<BufferSize>::available() {
  if(full) {
    return BufferSize;
  }
  return ((tail - head) + BufferSize) % BufferSize;
}

template<int BufferSize>
bool RS485Bus<BufferSize>::isBufferFull() {
  return full;
}

template<int BufferSize>
int RS485Bus<BufferSize>::fetch() {
  int bytesRead = 0;
  while(buffer.available() > 0 && !full) {
    bytesRead++;

    putByteInBuffer(buffer.read());
  }

  return bytesRead;
}

template<int BufferSize>
void RS485Bus<BufferSize>::putByteInBuffer(const unsigned char& value) {
  readBuffer[tail] = value;
  tail = (tail + 1) % BufferSize;

  if(tail == head) { // We've looped back around
    full = true;
  }
}

template<int BufferSize>
int RS485Bus<BufferSize>::operator[](const int& index) {
  if (index >= available()) {
    return -1;
  }
  int bufferPosition = (head + index) % BufferSize;
  return readBuffer[bufferPosition];
}

template<int BufferSize>
void RS485Bus<BufferSize>::setReadBackDelayMs(int milliseconds) {
  readBackRetryMilliseconds = milliseconds > 0 ? milliseconds : 0;
}
template<int BufferSize>
void RS485Bus<BufferSize>::setReadBackRetries(int retryCount) {
  readBackRetryCount =  retryCount > 0 ? retryCount : 0;
}