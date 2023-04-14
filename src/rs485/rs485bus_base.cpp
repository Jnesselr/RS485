#include "rs485/rs485bus_base.h"

RS485BusBase::RS485BusBase(BusIO& busIO, uint8_t readEnablePin, uint8_t writeEnablePin) :
  busIO(busIO),
  readEnablePin(readEnablePin),
  writeEnablePin(writeEnablePin) {
    pinMode(readEnablePin, OUTPUT);
    pinMode(writeEnablePin, OUTPUT);
    digitalWrite(readEnablePin, LOW);
    digitalWrite(writeEnablePin, LOW);
}

WriteResult RS485BusBase::write(uint8_t writeValue) {
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
    return WriteResult::NO_WRITE_BUFFER_FULL;
  } else if(anyBytesFetched) {
    return WriteResult::NO_WRITE_NEW_BYTES;
  }

  bool alreadySetToWrite = writeCurrentlyEnabled;

  if(! alreadySetToWrite) {
    enableWrite(true);
  }

  busIO.write(writeValue);
  delay(settleTimeMs);

  if(! alreadySetToWrite) {
    enableWrite(false);
  }

  bool readUnexpectedBytes = false;

  while(true) {
    bool bytesAvailable = (busIO.available() > 0);
    if(! bytesAvailable) {
      for(size_t i=0; i < readBackRetryCount; i++) {
        // delayMicroseconds(100);
        delay(readBackRetryMs);

        bytesAvailable |= (busIO.available() > 0);
        if(bytesAvailable) {
          break;
        }
      }
    }

    if(! bytesAvailable) {
      if(readUnexpectedBytes) {
        return WriteResult::FAILED_READ_BACK;
      } else {
        return WriteResult::NO_READ_TIMEOUT;
      }
    }

    if(full) {
      // Refuse to even read the byte, because if it's not the one we expect, we can't put it in the buffer.
      return WriteResult::READ_BUFFER_FULL;
    }

    int readValue = busIO.read();
    if(readValue == writeValue) {
      if(readUnexpectedBytes) {
        return WriteResult::UNEXPECTED_EXTRA_BYTES;
      }
      return WriteResult::OK;
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
  while(!full && busIO.available() > 0) {
    bytesRead++;

    putByteInBuffer(busIO.read());
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

void RS485BusBase::setSettleTime(ArduinoTime_t settleTime_ms) {
  this->settleTimeMs = settleTime_ms;
}

void RS485BusBase::enableWrite(bool writeEnabled) {
  if(writeEnabled && ! writeCurrentlyEnabled) {
    writeCurrentlyEnabled = writeEnabled;

    // delayMicroseconds(10);
    digitalWrite(writeEnablePin, HIGH);
    // delayMicroseconds(10);
    delay(settleTimeMs);
  } else if(! writeEnabled && writeCurrentlyEnabled) {
    writeCurrentlyEnabled = writeEnabled;

    // delayMicroseconds(10);
    digitalWrite(writeEnablePin, LOW);
    // delayMicroseconds(10);
    delay(settleTimeMs);
  }
}

RS485WriteEnable::RS485WriteEnable(RS485BusBase* bus): bus(bus) {
  bus->enableWrite(true);
}

RS485WriteEnable::~RS485WriteEnable() {
  bus->enableWrite(false);
}