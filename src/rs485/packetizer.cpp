#include "rs485/packetizer.h"

Packetizer::Packetizer(RS485BusBase& bus, const PacketInfo& packetInfo):
bus(&bus),  packetInfo(&packetInfo) {}

void Packetizer::setFilter(const Filter& filter) {
  this->filter = &filter;
  this->filterLookAhead = filter.lookAheadBytes();
  this->lastBusAvailable = 0;
}

void Packetizer::removeFilter() {
  this->filter = nullptr;
  this->filterLookAhead = 0;
  this->lastBusAvailable = 0;
}

void Packetizer::eatOneByte() {
  bus->read();
  lastBusAvailable--; // read removes one byte from the bus
  startIndex--; // Reset us so we'll be reading the first byte again next time
  recheckBitmap >>= 1;
}

void Packetizer::rejectByte(size_t location) {
  // Remove any "no" byte at the start
  if(startIndex == 0) {
    eatOneByte();
  } else {
    if(location < (sizeof(recheckBitmap) * 8)) {
      recheckBitmap |= (1L << location);
    }
  }
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

    bool packetFound = hasPacketInnerLoop();
    if(packetFound) {
      return true;
    }

    unsigned long currentMillis = millis();
    
    if(currentMillis - startTimeMs >= maxReadTimeout) {
      return false;  // We timed out trying to read a valid packet
    }
  }

  return false;
}

bool Packetizer::hasPacketInnerLoop() {
  for(startIndex = 0; startIndex < lastBusAvailable; startIndex++) {
    bool shouldCallIsPacket = true;
    
    if(startIndex < (sizeof(recheckBitmap) * 8)) {
      if((recheckBitmap & (1L << startIndex)) > 0 ) {
        shouldCallIsPacket = false;
      }
    }
    
    if(shouldCallIsPacket && this->filter != nullptr) {
      if(
        this->filter->isEnabled()
      ) {
        if(startIndex + this->filterLookAhead >= lastBusAvailable) {
          return false; // We don't have enough bytes to call this filter
        }

        shouldCallIsPacket = filter->preFilter(*bus, startIndex);
      }
    }

    if(! shouldCallIsPacket) {
      rejectByte(startIndex);
      continue;
    }

    int endIndex = lastBusAvailable - 1;
    PacketStatus status = packetInfo->isPacket(*bus, startIndex, endIndex);

    if(status == PacketStatus::NO) {
      rejectByte(startIndex);
    }
    else if(status == PacketStatus::YES) {
      packetSize = endIndex - startIndex + 1;

      // Clear out all bytes before startIndex
      while(startIndex > 0) {
        eatOneByte();
      }

      if(
        this->filter != nullptr &&
        this->filter->isEnabled() &&
        ! this->filter->postFilter(*bus, 0, packetSize)
      ) {
        clearPacket();

        /**
         * Things are in a weird state right now, which would normally be fixed when a packet is found
         * because the consumer would be calling hasPacket again. Instead, we break out of this inner
         * loop and claiming we didn't find a packet. The downside to this is that without filtering,
         * hasPacket may search the entire bus before checking the timeout. This will check it after
         * every packet. It's the same behavior as the consumer filtering out their own packets, but
         * it may cause some confusion.
         */
        return false;
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