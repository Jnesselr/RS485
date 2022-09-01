#pragma once

#include "rs485/protocol.h"

/**
 * This protocol is designed for the Lumen PnP feeders.
 * 
 * Lumen PNP GitHub: https://github.com/opulo-inc/lumenpnp
 * 
 * Packet format: <address:1> <length:1> <data:N> <checksum:2>
 */
class HippoFeedProtocol : public Protocol {
public:
  virtual PacketStatus isPacket(const RS485BusBase& bus, size_t startIndex, size_t& endIndex) const;
};