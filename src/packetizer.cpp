#include "packetizer.h"
#include <unity.h>

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

  unsigned long startMillis = millis();

  while(true) {
    int bytesFetched = bus->fetch();

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
    if(currentMillis - startMillis >= maxReadTimeout) {
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

  unsigned long currentMillis = millis();
  message = "Current read time: " + std::to_string(currentMillis);
  TEST_MESSAGE(message.c_str());
  if(currentMillis - lastByteReadTime < busQuietTime) {
    unsigned long delayTime = busQuietTime - (currentMillis - lastByteReadTime);

    while(true) {
      message = "Delaying: " + std::to_string(delayTime);
      TEST_MESSAGE(message.c_str());
      delay(delayTime);
      int bytesFetched = fetchFromBus();
      message = "Bytes fetched: " + std::to_string(bytesFetched);
      TEST_MESSAGE(message.c_str());
      if(bytesFetched == 0) {
        break;
      }

      delayTime = busQuietTime;
    }
  }

  for(int i = 0; i < bufferSize; i++) {
    WriteStatus status = bus->write(buffer[i]);
    message = std::to_string(i) + ": " + std::to_string((int)status);
    TEST_MESSAGE(message.c_str());
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
  std::string message;
  int result = bus->fetch();
  message = "Bytes fetched from bus: " + std::to_string(result);
  TEST_MESSAGE(message.c_str());
  if(result > 0) {
    lastByteReadTime = millis();
    message = "Last byte read time: " + std::to_string(lastByteReadTime);
    TEST_MESSAGE(message.c_str());
  }
  return result;
}