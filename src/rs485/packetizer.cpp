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
  lastBusAvailable--;  // read removes one byte from the bus
  startIndex--;  // Reset us so we'll be reading the first byte again next time
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

bool Packetizer::hasPacket() {
  TimeMicroseconds_t functionStartTime = micros();
  TimeMicroseconds_t lastPacketTime = functionStartTime;

  boolean hasPacket = hasPacketNow();

  while(true) {
    if(hasPacket and startIndex == 0) {
      return true;  // We have a packet aligned to the start of our bus, return it immediately.
    }

    fetchFromBus();

    size_t currentBusAvailable = bus->available();
    TimeMicroseconds_t currentTime = micros();

    if (lastBusAvailable == currentBusAvailable) {
      if(! hasPacket) {
        return false;  // If we have no new bytes and don't have a packet, exit early to be nice to the calling function.
      }

      // We do have a packet here
      if(falsePacketVerificationTimeout == 0) {  // We won't bother checking for any other packets
        return true;
      }
      TimeMicroseconds_t timeSinceLastPacket = currentTime - lastPacketTime;
      if(timeSinceLastPacket > falsePacketVerificationTimeout) {
        return true;
      } else {
        continue;  // No new bytes to check but we don't want time out just yet
      }
    }

    // We have new bytes
    TimeMicroseconds_t timeSinceFunctionStart = currentTime - functionStartTime;
    if(timeSinceFunctionStart > maxReadTimeout) {
      return hasPacket;  // Whatever we've found so far, we're letting the caller know about it
    }

    size_t oldStartIndex = startIndex;
    hasPacket = hasPacketNow();

    if(hasPacket && oldStartIndex != startIndex) {  // We have a new packet
      lastPacketTime = micros();
    }
  }
}

bool Packetizer::hasPacketNow() {
  size_t currentBusAvailable = bus->available();

  std::cout << "last vs current: " << lastBusAvailable << " " << currentBusAvailable << std::endl;

  if(lastBusAvailable == currentBusAvailable && !shouldRecheck) {
    if(endIndex > 0) {
      return true;  // We do have a packet and no extra bytes so no need to check again for a packet
    }
    return false;  // Don't bother rechecking our bus, we have the same number of bytes to work with and aren't forcing a recheck
  }

  lastBusAvailable = currentBusAvailable;

  shouldRecheck = false;  // We assume we don't need to force recheck next time, even if we did this time.
  endIndex = 0;  // If we had a packet, we can find it again

  for(startIndex = 0; startIndex < lastBusAvailable; startIndex++) {
    bool shouldCallIsPacket = true;
    
    if(startIndex < (sizeof(recheckBitmap) * 8)) {
      if((recheckBitmap & (1L << startIndex)) > 0 ) {
        shouldCallIsPacket = false;
      }
    }
    
    if(shouldCallIsPacket && this->filter != nullptr && this->filter->isEnabled()) {
      if(startIndex + this->filterLookAhead >= lastBusAvailable) {
        return false;  // We don't have enough bytes to call this filter and no further bytes will either
      }

      shouldCallIsPacket = filter->preFilter(*bus, startIndex);
    }

    if(! shouldCallIsPacket) {
      rejectByte(startIndex);
      continue;
    }

    IsPacketResult result = protocol->isPacket(*bus, startIndex, lastBusAvailable - 1);
    std::string status;
    switch(result.status) {
      case PacketStatus::NO:
      status = "No";
      break;
      case PacketStatus::NOT_ENOUGH_BYTES:
      status = "Not Enough Bytes";
      break;
      case PacketStatus::YES:
      status = "Yes";
      break;
    }

    std::cout << "is packet result (" << startIndex << ", " << lastBusAvailable - 1 << "): " << status << std::endl;

    if(result.status == PacketStatus::NO) {
      rejectByte(startIndex);
    }
    else if(result.status == PacketStatus::YES) {
      endIndex = startIndex + result.packetLength - 1;

      if(
        this->filter != nullptr &&
        this->filter->isEnabled() &&
        ! this->filter->postFilter(*bus, startIndex, endIndex)
      ) {
        if(startIndex == 0) {
          // If the packet is at the start and is filtered out, we know we can just discard the whole packet
          clearPacket();
          startIndex = -1;  // The loop increments after our continue below and we want it to start at 0, not 1 on the next loop.
        }

        endIndex = 0;  // Clear endIndex so it doesn't look like we have a packet since it was just filtered out

        continue;  // We may still have another valid packet, so continue checking.
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

  // This isn't super necessary since a packet existing is based on the endIndex, but not resetting this caused confusion during a debug session so we reset it if there is no packet.
  startIndex = 0;

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

void Packetizer::setFalsePacketVerificationTimeout(TimeMicroseconds_t falsePacketVerificationTimeout) {
  this->falsePacketVerificationTimeout = falsePacketVerificationTimeout;
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

  RS485WriteEnable writeEnable(bus);  // RAII to enable/disable bus writing

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