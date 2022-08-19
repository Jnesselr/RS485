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
  bool anyBytesFetched = fetch() > 0;
  bool newBytesFetched = anyBytesFetched;
  while(newBytesFetched) {
    for(int i=0; i <= preFetchRetryCount; i++) {
      delay(preFetchDelayMilliseconds);

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
  while(!full && buffer.available() > 0) {
    bytesRead++;

    putByteInBuffer(buffer.read());
  }

  return bytesRead;
}

int RS485BusBase::read() {
  fetch();
  if(available() == 0) {
    return -1;
  }

  int value = getByte(head);
  head = (head + 1) % readBufferSize;
  full = false;

  return value;
}

void RS485BusBase::putByteInBuffer(const unsigned char& value) {
  setByte(tail, value);
  tail = (tail + 1) % readBufferSize;

  if(tail == head) { // We've looped back around
    full = true;
  }
}

const int RS485BusBase::operator[](const unsigned int& index) const {
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

void RS485BusBase::setPreFetchDelayMs(int milliseconds) {
  preFetchDelayMilliseconds = milliseconds > 0 ? milliseconds : 0;
}
void RS485BusBase::setPreFetchRetries(int retryCount) {
  preFetchRetryCount =  retryCount > 0 ? retryCount : 0;
}