#pragma once

#include "rs485bus_base.h"

/**
 * Filters can be given to the Packetizer to either filter packets before (preFilter) or after (postFilter) the isPacket of the given Protocol class
 * runs.
 * 
 * This can be useful for a myriad of reasons. For example, say you know your packets always start with a specific byte such as a colon ':'. You can
 * use the FilterValueAtIndex implementation to look for that specific byte in your preFilter. This is especially useful if your isPacket implementation
 * is computation intensive. The same technique can be used if your packet has a constant address byte. Say your packet format's first byte was the target
 * address and that you only wanted values of 0xff and 0x35 to get through. Use FilterValueAtIndex with those two values. A preFilter doesn't guarantee
 * that something is a packet, it just lets the packetizer know to call isPacket.
 * 
 * There is one place where using a simple preFilter has worse performance than just calling isPacket. If your packets are consistent (meaning there's not
 * random bytes interrupting or coming in between packets from things like collisions) AND the values you're looking for are commonly seen inside a packet,
 * then isPacket may be called more than if you hadn't filtered anything. As an example, imagine our startIndex is on a packet after just having read one.
 * 
 * <previous packet> 07 03 12 12 12 40 <next packet>
 *                   ^^
 * 
 * If our preFilter was looking for 0x12 as its first byte (say an address), then isPacket would be called 3 times (one for each 0x12 above) which is more
 * than the one time it would have been called if we didn't preFilter. If you have any unexpected bytes interrupting packets, you're back to hunting for
 * the next packet regardless of filtering. This just puts it into the "hunting for next packet" state.
 * 
 * You can use postFilter when you need to know a set of bytes is a packet to do filtering. This is useful in protocols where you need to check one or
 * more values but you don't know where those values are until you have the entire packet available to you. It would also be useful in the example above
 * because the packet would have been read and then discarded before the consumer lost it, but tracking of packets would not have been lost.
 */
class Filter {
public:
  // How many bytes to look ahead? 0 means "only look at this byte". Applies to pre-filter only.
  virtual unsigned int lookAheadBytes() const = 0;

  // Packetizer will make sure this is always called where endIndex is at least "lookAheadBytes" away. Don't check past that byte.
  // Return true to call isPacket on this data. Return false and the packetizer will treat this startIndex as if isPacket returned NO.
  virtual bool preFilter(const RS485BusBase& bus, const int startIndex) const = 0;

  // Packetizer will call this after Protocol's isPacket returns true.
  // Return true to return this packet to the user.
  virtual bool postFilter(const RS485BusBase& bus, const int startIndex, const int endIndex) const = 0;

  virtual bool isEnabled() const { return enabled; }
  virtual void setEnabled(bool enabled = true) { this->enabled = enabled; };

protected:
  bool enabled = true;
};