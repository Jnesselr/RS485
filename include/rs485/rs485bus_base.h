#pragma once

#include "Arduino.h"
#include "util.h"
#include "read_write_buffer.h"

enum class WriteStatus {
  OK,                       // Read the written byte right back.
  UNEXPECTED_EXTRA_BYTES,   // Read extra bytes back but eventually found the one we wrote. See notes below.
  NO_READ_TIMEOUT,          // No bytes read at all, timed out waiting
  FAILED_READ_BACK,         // Read some bytes but failed to see our byte
  READ_BUFFER_FULL,         // Our internal buffer is full. Byte was still written, but we won't check it. See notes below.
  NO_WRITE_NEW_BYTES,       // We received new bytes on the bus with one of our fetch calls before writing. To avoid interrupting a message, we will not write.
  NO_WRITE_BUFFER_FULL      // Exactly the same as READ_BUFFER_FULL, but we could tell the buffer was full before we decided to write a byte.
};

/*
Notes on WriteStatus:
- UNEXPECTED_EXTRA_BYTES: The byte we read back might be one we've sent or it might be from another source. This layer of the library leaves it up to the caller to decide what to do. It may be that this is the first byte of a packet that you're sending and everything up to that byte is valid, so no collision has yet occurred. If it does belong to another sender, then the bus essentially ate it which is less ideal.
- READ_BUFFER_FULL: In the event we read back a value different from what we write, we put that value in an internal buffer for the caller to access later. It is the caller's responsibility to make sure this buffer is not full before they start writing, as well as making sure the ReadWriteBuffer has no bytes available by calling fetch(). However, if the reader keeps getting data available to it within the retries/timeout it will also fill up the buffer even if it was empty to start.
*/

class RS485BusBase {
public:
  explicit RS485BusBase(ReadWriteBuffer& buffer, uint8_t readEnablePin, uint8_t writeEnablePin);

  virtual size_t bufferSize() const = 0;

  VIRTUAL_FOR_UNIT_TEST WriteStatus write(uint8_t value);

  VIRTUAL_FOR_UNIT_TEST size_t available() const;
  bool isBufferFull() const;
  VIRTUAL_FOR_UNIT_TEST size_t fetch();  // Returns how many bytes were fetched
  int16_t read();  // Reads one byte and returns it. Returns -1 if no byte is read

  VIRTUAL_FOR_UNIT_TEST int16_t operator[](size_t index) const;

  void setReadBackDelay(ArduinoTime_t delay_ms);
  void setReadBackRetries(size_t retryCount);
  void setPreFetchDelay(ArduinoTime_t delay_ms);
  void setPreFetchRetries(size_t retryCount);

protected:
  virtual void setByte(size_t bufferIndex, uint8_t value) = 0;
  virtual uint8_t getByte(size_t bufferIndex) const = 0;
  size_t readBufferSize;

private:
  void putByteInBuffer(uint8_t value);

  ReadWriteBuffer& buffer;
  uint8_t readEnablePin;
  uint8_t writeEnablePin;

  size_t head = 0;
  size_t tail = 0;
  bool full = false;

  ArduinoTime_t readBackRetryMs = 0;
  size_t readBackRetryCount = 0;
  ArduinoTime_t preFetchDelayMs = 0;
  size_t preFetchRetryCount = 0;
};
