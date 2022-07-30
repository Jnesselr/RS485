#include "packetizer.h"

Packetizer::Packetizer(RS485BusBase& bus): bus(&bus) {}

bool Packetizer::hasPacket() {
  return false;
}