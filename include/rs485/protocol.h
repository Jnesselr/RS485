#pragma once

#include "rs485bus_base.h"

enum class PacketStatus: uint8_t {
  YES,
  NO,
  NOT_ENOUGH_BYTES
};

struct IsPacketResult {
  PacketStatus status;
  size_t packetLength;
};

class Protocol {
public:
  virtual ~Protocol() {}

  /**
   * This method overall is pretty simple. You can inspect the bytes on the bus from startIndex up to and including endIndex.
   * - Return status YES and the packet length only if the packet starts at startIndex.
   * - Return status NO if you can guarantee this isn't a packet and never will be no matter how much more data you see.
   * - Return NOT_ENOUGH_BYTES if this may be a packet, and more bytes might help you know for sure. It will be tested again
   *     with a new endIndex. Return NO if you can be sure the number of bytes needed is larger than the bus buffer size.
   * 
   * Only a status of YES should have a non-zero packet length.
   * 
   * As far as the packetizer is concerned, it will only test packets that return NO, once (This only applies to the first 64
   * bytes). If there is a NO at the beginning of the buffer, then that byte is discarded from the buffer. If NOT_ENOUGH_BYTES
   * is returned, then it will test it again when more bytes are available. If NOT_ENOUGH_BYTES is returned and the buffer is
   * full up, then NOT_ENOUGH_BYTES will be treated as a NO. It is helpful for Protocol implementations to check for overall
   * buffer length. If a length field is available and the length field is larger than the buffer size, it's more effecient to
   * just return NO.
   * 
   * Finally, if at any point a YES is returned, all bytes previous to that are discarded.
   * 
   * In an ideal world, the next byte after the updated endIndex, when Protocol returns YES, would be the start of a new packet.
   * Unfortunately, given collisions and other issues, that may not be the case. Do not make any assumptions or try to "read ahead".
   */
  virtual IsPacketResult isPacket(const RS485BusBase& bus, size_t startIndex, size_t endIndex) const = 0;
};