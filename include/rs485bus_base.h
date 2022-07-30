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

class RS485BusBase {
public:
  explicit RS485BusBase(ReadWriteBuffer& buffer, int readEnablePin, int writeEnablePin);

  virtual int bufferSize() = 0;

  WriteStatus write(const unsigned char& value);

  int available();
  bool isBufferFull();
  int fetch();  // Returns how many bytes were fetched

  const int operator[](const int& index);

  void setReadBackDelayMs(int milliseconds);
  void setReadBackRetries(int retryCount);

protected:
  virtual void setByte(const int& index, const unsigned char value) = 0;
  virtual const unsigned char getByte(const int& index) = 0;
  int readBufferSize;

private:
  void putByteInBuffer(const unsigned char& value);

  ReadWriteBuffer& buffer;
  int readEnablePin;
  int writeEnablePin;

  int head = 0;
  int tail = 0;
  bool full = false;

  int readBackRetryMilliseconds = 0;
  int readBackRetryCount = 0;
};
