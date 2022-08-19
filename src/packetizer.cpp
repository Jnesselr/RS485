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
  bus->fetch();

  if(lastBusAvailable == bus->available()) {
    return false; // No new bytes are available, so no new packet is available
  }

  lastBusAvailable = bus->available();

  for(startIndex = 0; startIndex < lastBusAvailable; startIndex++) {
    bool alreadyChecked = (recheckBitmap & (1 << startIndex)) > 0;
    if(alreadyChecked) {
      continue;
    }

    int endIndex = lastBusAvailable - 1;
    PacketStatus status = packetInfo->isPacket(*bus, startIndex, endIndex);

    if(status == PacketStatus::NO) {
      recheckBitmap |= (1 << startIndex);
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