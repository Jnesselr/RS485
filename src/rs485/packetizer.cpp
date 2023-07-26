#include "rs485/packetizer.h"

Packetizer::Packetizer(RS485BusBase& bus, const Protocol& protocol):
bus(&bus),  protocol(&protocol) {}

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

  TimeMicroseconds_t startTime = micros();

  while(true) {
    fetchFromBus();

    size_t currentBusAvailable = bus->available();

    if(lastBusAvailable != currentBusAvailable) {
      lastBusAvailable = currentBusAvailable;

      bool packetFound = hasPacketInnerLoop();
      if(packetFound) {
        return true;
      }
    }

    TimeMicroseconds_t currentTimeMs = micros();
    
    if((currentTimeMs - startTime) >= maxReadTimeout) {
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

    size_t endIndex = lastBusAvailable - 1;
    IsPacketResult result = protocol->isPacket(*bus, startIndex, endIndex);

    if(result.status == PacketStatus::NO) {
      rejectByte(startIndex);
    }
    else if(result.status == PacketStatus::YES) {
      packetSize = result.packetLength;

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
         * loop and claim we didn't find a packet. The downside to this is that without filtering,
         * hasPacket may search the entire bus before checking the timeout. This will check it after
         * every packet. It's the same behavior as the consumer filtering out their own packets, but
         * it may cause some confusion.
         */
        return false;
      }

      return true;
    }
    else if(result.status == PacketStatus::NOT_ENOUGH_BYTES) {
      // Remove any "not enough bytes" byte at the start, only if the buffer is full
      if(startIndex == 0 && bus->isBufferFull()) {
        eatOneByte();
      }
    }
  }

  return false;
}

Packet Packetizer::getPacket() {
  if(packetSize > 0) {
    return {
      .startIndex = 0,
      .endIndex = packetSize - 1,
    };
  } else {
    return {
      .startIndex = 0,
      .endIndex = 0,
    };
  }
}

size_t Packetizer::packetLength() {
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

void Packetizer::setMaxReadTimeout(TimeMicroseconds_t maxReadTimeout) {
  this->maxReadTimeout = maxReadTimeout;
}

PacketWriteResult Packetizer::writePacket(const uint8_t* buffer, size_t bufferSize) {
  TimeMicroseconds_t startTime = micros();
  if((startTime - lastByteReadTimestamp) < busQuietTime) {
    TimeMicroseconds_t delayTime = busQuietTime - (startTime - lastByteReadTimestamp);

    while(true) {
      delayMicroseconds(delayTime);
      size_t bytesFetched = fetchFromBus();
      if(bytesFetched == 0) {
        break;
      }

      if(lastByteReadTimestamp >= startTime + maxWriteTimeout) {
        return PacketWriteResult::FAILED_TIMEOUT;
      }

      delayTime = busQuietTime;
    }
  }

  RS485WriteEnable writeEnable(bus); // RAII to enable/disable bus writing

  for(size_t i = 0; i < bufferSize; i++) {
    WriteResult status = bus->write(buffer[i]);
    switch(status) {
      case WriteResult::OK:
        continue;
      case WriteResult::UNEXPECTED_EXTRA_BYTES:
        if(i > 0) {
          return PacketWriteResult::FAILED_INTERRUPTED;
        }
        break;
      case WriteResult::READ_BUFFER_FULL:
      case WriteResult::NO_WRITE_BUFFER_FULL:
        return PacketWriteResult::FAILED_BUFFER_FULL;
      case WriteResult::NO_READ_TIMEOUT:
      case WriteResult::FAILED_READ_BACK:
      case WriteResult::NO_WRITE_NEW_BYTES:
        return PacketWriteResult::FAILED_INTERRUPTED;
    }
  }

  return PacketWriteResult::OK;
}

void Packetizer::setMaxWriteTimeout(TimeMicroseconds_t maxWriteTimeout) {
  this->maxWriteTimeout = maxWriteTimeout;
}

void Packetizer::setBusQuietTime(TimeMicroseconds_t busQuietTime) {
  this->busQuietTime = busQuietTime;
}

size_t Packetizer::fetchFromBus() {
  int16_t result = bus->fetch();
  if(result > 0) {
    lastByteReadTimestamp = micros();
  }
  return result;
}