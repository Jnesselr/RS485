#pragma once

#include "inttypes.h"
#include "rs485/rs485bus_base.h"
#include "rs485/protocol.h"
#include "rs485/filter.h"

enum class PacketWriteResult {
  OK,                   // Writing all bytes succeeded
  FAILED_INTERRUPTED,   // We tried to write out our packet, but we were interrupted before finishing
  FAILED_BUFFER_FULL,   // Our bus' buffer became full, so we can't write anymore bytes
  FAILED_TIMEOUT        // The bus wasn't quiet enough for us for long enough to start writing our packet
};

/**
 * The Packetizer class wraps the RS485 Bus, allowing you to read and write packets more effeciently. While you don't have to
 * write anything using this class and can still use reading/writing on the bus directly, you must specify a Protocol
 * implementation which is used for reading.
 *
 * If all you wanted to do was read packets in a tight loop, you would do something like this:
 * Packetizer packetizer(bus, protocol);
 * while(true) {
 *   if(packetizer.hasPacket()) {
 *     // bus[0] to bus[packetizer.packetLength() - 1] is your packet. Do with it what you need to.
 *     // Clear the packet out so the next one can be read when hasPacket is next called.
 *     packetizer.clearPacket();
 *   }
 * }
 *
 * Writing a packet will attempt to write one byte at a time, verifying that that byte gets written to the bus. The
 * packet you write does not have to be a valid byte according to the protocol. The write method will block until
 * the max write timeout is reached if it sees bytes on the buffer during the quiet time. It will only attempt to
 * write the packet once though, so it is up to the consumer to handle any issues/retries.
 */
class Packetizer {
public:
  explicit Packetizer(RS485BusBase& bus, const Protocol& protocol);

  // Fetch bytes from the bus and see if a packet is available based on the Protocol.
  bool hasPacket();
  // The length of the packet available if hasPacket returns true. 0 otherwise.
  size_t packetLength();
  // Clear the packet. If a packet is found, further calls to hasPacket will return true until this method is called.
  void clearPacket();

  /**
   * How long to keep trying to keep trying to read a packet. If no new data is available, this value is irrelevent.
   * If this is too high and the bus is constantly receiving new data, then hasPacket will block.
   */
  void setMaxReadTimeout(TimeMicroseconds_t maxReadTimeout);

  PacketWriteResult writePacket(const uint8_t* buffer, size_t bufferSize);
  
  // Before attempting to write a packet, how long should the bus not receive any new bytes
  void setBusQuietTime(TimeMicroseconds_t busQuietTime);
  // The maximum amount of time we are willing to wait for the bus to go quiet
  void setMaxWriteTimeout(TimeMicroseconds_t maxWriteTimeout);

  // Add a filter to this packetizer. See the Filter class for more details
  void setFilter(const Filter& filter);
  // Remove a filter from this packetizer
  void removeFilter();
private:
  bool hasPacketInnerLoop();

  size_t fetchFromBus();
  inline void eatOneByte();
  inline void rejectByte(size_t location);

  RS485BusBase* bus;
  const Protocol* protocol;
  size_t packetSize = 0;

  const Filter* filter = nullptr;
  size_t filterLookAhead = 0;

  bool shouldRecheck = true;
  size_t lastBusAvailable = 0;
  size_t startIndex = 0;
  uint64_t recheckBitmap = 0;

  TimeMicroseconds_t maxReadTimeout = -1;
  TimeMicroseconds_t maxWriteTimeout = -1;

  TimeMicroseconds_t lastByteReadTimestamp = 0;  // Last time any bytes were known to be fetched
  TimeMicroseconds_t busQuietTime = 0;  // How long the bus needs to go without fetching a byte
};