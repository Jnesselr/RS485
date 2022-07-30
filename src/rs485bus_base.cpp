#include "rs485bus_base.h"

RS485BusBase::RS485BusBase(ReadWriteBuffer& buffer, int readEnablePin, int writeEnablePin) :
  buffer(buffer),
  readEnablePin(readEnablePin),
  writeEnablePin(writeEnablePin) {
    pinMode(readEnablePin, OUTPUT);
    pinMode(writeEnablePin, OUTPUT);
    digitalWrite(readEnablePin, LOW);
    digitalWrite(writeEnablePin, LOW);
}

WriteStatus RS485BusBase::write(const unsigned char& writeValue) {
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

int RS485BusBase::available() const {
  if(full) {
    return readBufferSize;
  }
  return ((tail - head) + readBufferSize) % readBufferSize;
}

bool RS485BusBase::isBufferFull() const {
  return full;
}

int RS485BusBase::fetch() {
  int bytesRead = 0;
  while(buffer.available() > 0 && !full) {
    bytesRead++;

    putByteInBuffer(buffer.read());
  }

  return bytesRead;
}

void RS485BusBase::putByteInBuffer(const unsigned char& value) {
  setByte(tail, value);
  tail = (tail + 1) % readBufferSize;

  if(tail == head) { // We've looped back around
    full = true;
  }
}

const int RS485BusBase::operator[](const int& index) const {
  if (index >= available()) {
    return -1;
  }
  int bufferPosition = (head + index) % readBufferSize;
  return getByte(bufferPosition);
}

void RS485BusBase::setReadBackDelayMs(int milliseconds) {
  readBackRetryMilliseconds = milliseconds > 0 ? milliseconds : 0;
}

void RS485BusBase::setReadBackRetries(int retryCount) {
  readBackRetryCount =  retryCount > 0 ? retryCount : 0;
}