#include "rs485/packetizer.h"
#include <gtest/gtest.h>
#include <gtest/gtest-message.h>

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
  if(endIndex > 0) {
    endIndex--;  // Check for > 0 to handle both with and without packet cases.
  }
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

/**
bool Packetizer::hasPacket() {
  if(endIndex > 0) {
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
*/

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

    IsPacketResult result = protocol->isPacket(*bus, startIndex, lastBusAvailable - 1);

    if(result.status == PacketStatus::NO) {
      rejectByte(startIndex);
    }
    else if(result.status == PacketStatus::YES) {
      endIndex = startIndex + result.packetLength - 1;

      // Clear out all bytes before startIndex
      while(startIndex > 0) {
        eatOneByte();
      }

      if(
        this->filter != nullptr &&
        this->filter->isEnabled() &&
        ! this->filter->postFilter(*bus, startIndex, endIndex)
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

bool Packetizer::hasPacketNow() {
  size_t currentBusAvailable = bus->available();

  if(lastBusAvailable == currentBusAvailable && !shouldRecheck) {
    if(endIndex > 0) {
      return true; // We do have a packet and no extra bytes so no need to check again for a packet
    }
    // ADD_FAILURE() << "No rechecky for you";
    return false;  // Don't bother rechecking our bus, we have the same number of bytes to work with and aren't forcing a recheck
  }

  lastBusAvailable = currentBusAvailable;

  shouldRecheck = false; // We assume we don't need to force recheck next time, even if we did this time.
  endIndex = 0; // If we had a packet, we can find it again

  for(startIndex = 0; startIndex < lastBusAvailable; startIndex++) {
    // ADD_FAILURE() << "Loop: (" << startIndex << ", " << (lastBusAvailable - 1) << ")";
    bool shouldCallIsPacket = true;
    
    if(startIndex < (sizeof(recheckBitmap) * 8)) {
      if((recheckBitmap & (1L << startIndex)) > 0 ) {
        shouldCallIsPacket = false;
      }
    }
    
    if(shouldCallIsPacket && this->filter != nullptr && this->filter->isEnabled()) {
      if(startIndex + this->filterLookAhead >= lastBusAvailable) {
        // ADD_FAILURE() << "Pre filtered out!";
        return false; // We don't have enough bytes to call this filter and no further bytes will either
      }

      shouldCallIsPacket = filter->preFilter(*bus, startIndex);
    }

    if(! shouldCallIsPacket) {
      rejectByte(startIndex);
      continue;
    }

    IsPacketResult result = protocol->isPacket(*bus, startIndex, lastBusAvailable - 1);

    if(result.status == PacketStatus::NO) {
      rejectByte(startIndex);
    }
    else if(result.status == PacketStatus::YES) {
      endIndex = startIndex + result.packetLength - 1;
      // ADD_FAILURE() << "Found a packet: (" << startIndex << ", " << endIndex << ")";

      // Clear out all bytes before startIndex (TODO This wil mess up our wait timer)

      if(
        this->filter != nullptr &&
        this->filter->isEnabled() &&
        ! this->filter->postFilter(*bus, startIndex, endIndex)
      ) {
        clearPacket();  // We have a packet, we just don't want it.
        // ADD_FAILURE() << "Filtered out! Remaining bus available() " << bus->available() << " start index  " << startIndex;
        startIndex = -1; // We need it to wrap around to 0 after incrementing

        continue;  // We may still have another valid packet, so continue checking.
      }

      // ADD_FAILURE() << "Not filtered out!";

      return true;
    }
    else if(result.status == PacketStatus::NOT_ENOUGH_BYTES) {
      // Remove any "not enough bytes" byte at the start, only if the buffer is full
      if(startIndex == 0 && bus->isBufferFull()) {
        eatOneByte();
      }
    }
  }

  // ADD_FAILURE() << "Awww, we got to the end";
  return false;
}

Packet Packetizer::getPacket() {
  if(endIndex > 0) {
    return {
      .startIndex = startIndex,
      .endIndex = endIndex,
    };
  } else {
    return {
      .startIndex = 0,
      .endIndex = 0,
    };
  }
}

void Packetizer::clearPacket() {
  if(endIndex == 0) {
    return;
  }

  while(endIndex != 0) {
    eatOneByte();
  }
  eatOneByte();  // endIndex will be 0 with one byte remaining to eat after the previous loop

  startIndex = 0;  // Force start index to zero since eating the bytes probably wrapped it around to a very large value.
  shouldRecheck = true;
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