#include "packetizer.h"

Packetizer::Packetizer(RS485BusBase& bus, const PacketInfo& packetInfo):
bus(&bus),  packetInfo(&packetInfo) {}

void Packetizer::eatOneByte() {
  bus->read();
  lastBusAvailable--; // read removes one byte from the bus
  startIndex--; // Reset us so we'll be reading the first byte again next time
  recheckBitmap >>= 1;
}

bool Packetizer::hasPacket() {
  if(packetSize > 0) {
    return true;  // We have a packet already, no need to do any searching
  }

  unsigned long startTimeMs = millis();

  while(true) {
    int bytesFetched = fetchFromBus();

    if(lastBusAvailable == bus->available()) {
      return false; // No new bytes are available, so no new packet is available
    }

    lastBusAvailable = bus->available();

    for(startIndex = 0; startIndex < lastBusAvailable; startIndex++) {
      bool alreadyChecked = false;
      
      if(startIndex < (sizeof(recheckBitmap) * 8)) {
        alreadyChecked = (recheckBitmap & (1L << startIndex)) > 0;
      }
      if(alreadyChecked) {
        // It's very possible to have already checked a byte
        if(startIndex == 0) {
          eatOneByte();
        }
        continue;
      }

      int endIndex = lastBusAvailable - 1;
      PacketStatus status = packetInfo->isPacket(*bus, startIndex, endIndex);

      if(status == PacketStatus::NO) {
        if(startIndex < (sizeof(recheckBitmap) * 8)) {
          recheckBitmap |= (1L << startIndex);
        }
        // Remove any "no" byte at the start
        if(startIndex == 0) {
          eatOneByte();
        }
      }
      else if(status == PacketStatus::YES) {
        packetSize = endIndex - startIndex + 1;

        // Clear out all bytes before startIndex
        while(startIndex > 0) {
          eatOneByte();
        }

        return true;
      }
      else if(status == PacketStatus::NOT_ENOUGH_BYTES) {
        // Remove any "not enough bytes" byte at the start, only if the buffer is full
        if(startIndex == 0 && bus->isBufferFull()) {
          eatOneByte();
        }
      }
    }

    unsigned long currentMillis = millis();
    
    if(currentMillis - startTimeMs >= maxReadTimeout) {
      return false;  // We timed out trying to read a valid packet
    }
  }

  return false;
}

int Packetizer::packetLength() {
  return packetSize;
}

void Packetizer::clearPacket() {
  if(packetSize == 0) {
    return;
  }

  while(packetSize > 0) {
    eatOneByte();
    packetSize--;
  }

  // Since nothing after our packet got checked, we reset this to 0 so it forcefully check new data
  lastBusAvailable = 0;
}

void Packetizer::setMaxReadTimeout(unsigned long maxReadTimeout) {
  this->maxReadTimeout = maxReadTimeout;
}

PacketWriteStatus Packetizer::writePacket(const unsigned char* buffer, int bufferSize) {
  std::string message;

  unsigned long startTimeMs = millis();
  if(startTimeMs - lastByteReadTime < busQuietTime) {
    unsigned long delayTime = busQuietTime - (startTimeMs - lastByteReadTime);

    while(true) {
      delay(delayTime);
      int bytesFetched = fetchFromBus();
      if(bytesFetched == 0) {
        break;
      }

      if(lastByteReadTime >= startTimeMs + maxWriteTimeout) {
        return PacketWriteStatus::FAILED_TIMEOUT;
      }

      delayTime = busQuietTime;
    }
  }

  for(int i = 0; i < bufferSize; i++) {
    WriteStatus status = bus->write(buffer[i]);
    if(status == WriteStatus::UNEXPECTED_EXTRA_BYTES) {
      if(i > 0) {
        return PacketWriteStatus::FAILED_INTERRUPTED;
      }
    } else if(status == WriteStatus::READ_BUFFER_FULL) {
      return PacketWriteStatus::FAILED_BUFFER_FULL;
    } else if(status == WriteStatus::NO_WRITE_BUFFER_FULL) {
      return PacketWriteStatus::FAILED_BUFFER_FULL;
    } else if(status == WriteStatus::NO_READ_TIMEOUT) {
      return PacketWriteStatus::FAILED_INTERRUPTED;
    } else if(status == WriteStatus::FAILED_READ_BACK) {
      return PacketWriteStatus::FAILED_INTERRUPTED;
    } else if(status == WriteStatus::NO_WRITE_NEW_BYTES) {
      return PacketWriteStatus::FAILED_INTERRUPTED;
    }
  }

  return PacketWriteStatus::OK;
}

void Packetizer::setMaxWriteTimeout(unsigned long maxWriteTimeout) {
  this->maxWriteTimeout = maxWriteTimeout;
}

void Packetizer::setBusQuietTime(unsigned int busQuietTime) {
  this->busQuietTime = busQuietTime;
}

int Packetizer::fetchFromBus() {
  int result = bus->fetch();
  if(result > 0) {
    lastByteReadTime = millis();
  }
  return result;
}