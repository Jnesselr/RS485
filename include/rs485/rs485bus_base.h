#pragma once

#include "Arduino.h"
#include "util.h"
#include "bus_io.h"


/*
Extra notes on WriteResult:
- UNEXPECTED_EXTRA_BYTES: The byte we read back might be one we've sent or it might be from another source. This layer of
  the library leaves it up to the caller to decide what to do. It may be that this is the first byte of a packet that you're
  sending and everything up to that byte is valid, so no collision has yet occurred. If it does belong to another sender,
  then the bus essentially ate it which is less ideal.
- READ_BUFFER_FULL: In the event we read back a value different from what we write, we put that value in an internal buffer
  for the caller to access later. It is the caller's responsibility to make sure this buffer is not full before they start
  writing, as well as making sure the BusIO has no bytes available by calling fetch(). However, if the reader
  keeps getting data available to it within the retries/timeout it will also fill up the buffer even if it was empty to start.
*/
enum class WriteResult {
  OK,                       // Read the written byte right back.
  UNEXPECTED_EXTRA_BYTES,   // Read extra bytes back but eventually found the one we wrote. See notes below.
  NO_READ_TIMEOUT,          // No bytes read at all, timed out waiting
  FAILED_READ_BACK,         // Read some bytes but failed to see our byte
  READ_BUFFER_FULL,         // Our internal buffer is full. Byte was still written, but we won't check it. See notes below.
  NO_WRITE_NEW_BYTES,       // We received new bytes on the bus with one of our fetch calls before writing. To avoid interrupting a message, we will not write.
  NO_WRITE_BUFFER_FULL      // Exactly the same as READ_BUFFER_FULL, but we could tell the buffer was full before we decided to write a byte.
};

/**
 * Direct access to the RS485 Bus. More than likely, consumers will create an instance of the RS485Bus instead of using this
 * partial class. Most consumers will also generallyl just use that to instatiate the Packetizer instead of using the bus
 * directly.
 * 
 * The RS485 bus is essentially two separate classes to get around partial templating issues. I wanted the RS485Bus to have
 * a template paramter to statically define the buffer size. The caveat to this is that anything that referenced that bus
 * would also have to know the bus size and probably would end up templated. Instead, they can reference the non-templated
 * RS485BusBase class here and be passed in the templated version. I could have written this to make the consumer define
 * a buffer pointer and length, but I didn't like how that looked so went with this approach instead.
 */
class RS485BusBase {
public:
  explicit RS485BusBase(BusIO& buffer, uint8_t readEnablePin, uint8_t writeEnablePin);

  virtual size_t bufferSize() const = 0;

  // Try to write a byte to the RS485 bus, verifying if it was written correctly
  VIRTUAL_FOR_UNIT_TEST WriteResult write(uint8_t value);

  // How many bytes are available in our internal buffer. Note that this is not the same as how many bytes are available through the bus IO
  VIRTUAL_FOR_UNIT_TEST size_t available() const;
  bool isBufferFull() const;
  // Fetch however many bytes we can from our bus IO, returning how many bytes we just fetched
  VIRTUAL_FOR_UNIT_TEST size_t fetch();
  // Reads one byte from our internal buffer and returns it. Calls fetch first. Returns -1 if no byte is available.
  int16_t read();

  // For filters and protocols, this is how to view data inside our internal buffer.
  VIRTUAL_FOR_UNIT_TEST int16_t operator[](size_t index) const;

  // How long to wait between read attempts to read back our written byte.
  void setReadBackDelay(ArduinoTime_t delay_ms);
  // How many times will we try to read back our byte before giving up.
  void setReadBackRetries(size_t retryCount);
  // If we see new bytes right before we attempt to write a byte, how long do we wait before checking again.
  void setPreFetchDelay(ArduinoTime_t delay_ms);
  // How many times do we recheck for new bytes before giving up and not writing our byte.
  void setPreFetchRetries(size_t retryCount);
  /*
  How long to wait after enabling the write pin, after writing, and after disabling the write pin.
  The same value is used for all 3. Setting this value too low prevents writes from being read back.
  */
  void setSettleTime(ArduinoTime_t settleTime_ms);

protected:
  virtual void setByte(size_t bufferIndex, uint8_t value) = 0;
  virtual uint8_t getByte(size_t bufferIndex) const = 0;
  size_t readBufferSize;

private:
  void putByteInBuffer(uint8_t value);

  BusIO& busIO;
  uint8_t readEnablePin;
  uint8_t writeEnablePin;

  size_t head = 0;
  size_t tail = 0;
  bool full = false;

  ArduinoTime_t readBackRetryMs = 0;
  size_t readBackRetryCount = 0;
  ArduinoTime_t preFetchDelayMs = 0;
  size_t preFetchRetryCount = 0;
  ArduinoTime_t settleTimeMs = 2;
};
