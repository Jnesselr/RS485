#include "rs485/rs485bus_base.h"

RS485BusBase::RS485BusBase(ReadWriteBuffer& buffer, uint8_t readEnablePin, uint8_t writeEnablePin) :
  buffer(buffer),
  readEnablePin(readEnablePin),
  writeEnablePin(writeEnablePin) {
    pinMode(readEnablePin, OUTPUT);
    pinMode(writeEnablePin, OUTPUT);
    digitalWrite(readEnablePin, LOW);
    digitalWrite(writeEnablePin, LOW);
}

WriteStatus RS485BusBase::write(uint8_t writeValue) {
  bool anyBytesFetched = fetch() > 0;
  bool newBytesFetched = anyBytesFetched;
  while(newBytesFetched) {
    for(size_t i=0; i <= preFetchRetryCount; i++) {
      delay(preFetchDelayMs);

      newBytesFetched = fetch() > 0;
      if(newBytesFetched) {
        break;
      }
    }
  }

  if(full) {
    return WriteStatus::NO_WRITE_BUFFER_FULL;
  } else if(anyBytesFetched) {
    return WriteStatus::NO_WRITE_NEW_BYTES;
  }

  digitalWrite(writeEnablePin, HIGH);

  buffer.write(writeValue);

  digitalWrite(writeEnablePin, LOW);

  bool readUnexpectedBytes = false;

  while(true) {
    bool bytesAvailable = (buffer.available() > 0);
    if(! bytesAvailable) {
      for(int i=0; i < readBackRetryCount; i++) {
        delay(readBackRetryMs);

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

size_t RS485BusBase::available() const {
  if(full) {
    return readBufferSize;
  }
  return ((tail - head) + readBufferSize) % readBufferSize;
}

bool RS485BusBase::isBufferFull() const {
  return full;
}

size_t RS485BusBase::fetch() {
  size_t bytesRead = 0;
  while(!full && buffer.available() > 0) {
    bytesRead++;

    putByteInBuffer(buffer.read());
  }

  return bytesRead;
}

int16_t RS485BusBase::read() {
  fetch();
  if(available() == 0) {
    return -1;
  }

  uint8_t value = getByte(head);
  head = (head + 1) % readBufferSize;
  full = false;

  return value;
}

void RS485BusBase::putByteInBuffer(uint8_t value) {
  setByte(tail, value);
  tail = (tail + 1) % readBufferSize;

  if(tail == head) { // We've looped back around
    full = true;
  }
}

int16_t RS485BusBase::operator[](size_t index) const {
  if (index >= available()) {
    return -1;
  }
  int bufferPosition = (head + index) % readBufferSize;
  return getByte(bufferPosition);
}

void RS485BusBase::setReadBackDelay(ArduinoTime_t delay_ms) {
  this->readBackRetryMs = delay_ms;
}

void RS485BusBase::setReadBackRetries(size_t retryCount) {
  this->readBackRetryCount = retryCount;
}

void RS485BusBase::setPreFetchDelay(ArduinoTime_t delay_ms) {
  this->preFetchDelayMs = delay_ms;
}
void RS485BusBase::setPreFetchRetries(size_t retryCount) {
  this->preFetchRetryCount = retryCount;
}