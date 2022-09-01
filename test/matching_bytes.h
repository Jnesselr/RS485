#pragma once

#include "rs485/protocol.h"

/**
 * This is a simple protocol example used in tests.
 * 
 * Returns:
 * - Yes if bus[startIndex] is found somewhere between (startIndex+1) and endIndex on the bus
 * - Not Enough Bytes if bus[startIndex] is even
 * - No if bus[startIndex] is odd
 */
class ProtocolMatchingBytes: public Protocol {
public:
  PacketStatus isPacket(const RS485BusBase& bus, const int startIndex, int& endIndex) const;
};