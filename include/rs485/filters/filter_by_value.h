#pragma once

#include "rs485/filter.h"
#include <inttypes.h>

class FilterByValue;

/**
 * ValueFilter is a helper class to FilterByValue. It allows you to filter bytes on both the pre and post filter
 * using a common interface.
 *
 * By default, all bytes are rejected.
 */
class ValueFilter {
public:
  ValueFilter();
  ValueFilter(const ValueFilter&) = delete;
  ValueFilter& operator=(const ValueFilter&) = delete;

  // Allow all bytes. Useful if you only care about enabling only the pre or post filter.
  void allowAll();
  // Reject all bytes. (default)
  void rejectAll();
  // Allow a specific byte.
  void allow(uint8_t byte);
  // Reject a specific byte. Useful to allow all BUT certain bytes.
  void reject(uint8_t byte);
private:
  void set(uint8_t value);
  void clear(uint8_t value);
  bool isSet(uint8_t value) const;

  uint8_t values[32];

  friend class FilterByValue;
};

/**
 * Filters packets out by value, at the offset specified in the constructor. By default the look ahead is 0,
 * which means the first byte of any potential packet is checked against whatever values are allowed in the
 * preValues and postValues filters. By default, all bytes are rejected.
 */
class FilterByValue: public Filter {
public:
  FilterByValue(): FilterByValue(0) {}
  FilterByValue(size_t lookAhead);

  virtual size_t lookAheadBytes() const;

  virtual bool preFilter(const RS485BusBase& bus, size_t startIndex) const;

  virtual bool postFilter(const RS485BusBase& bus, size_t startIndex, size_t endIndex) const;

  ValueFilter preValues;
  ValueFilter postValues;
private:
  size_t lookAhead;
};