#pragma once

#include "rs485/filter.h"
#include <inttypes.h>

class FilterByValue;

struct ValueFilter {
  ValueFilter();
  ValueFilter(const ValueFilter&) = delete;
  ValueFilter& operator=(const ValueFilter&) =delete;

  void allowAll();
  void rejectAll();
  void allow(uint8_t byte);
  void reject(uint8_t byte);
private:
  void set(uint8_t value);
  void clear(uint8_t value);
  bool isSet(uint8_t value) const;

  uint8_t values[32];

  friend class FilterByValue;
};

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