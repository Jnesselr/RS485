#include "filters/filter_by_value.h"

FilterByValue::FilterByValue(unsigned int lookAhead):
  lookAhead(lookAhead) {
}

unsigned int FilterByValue::lookAheadBytes() const {
  return lookAhead;
}

bool FilterByValue::preFilter(const RS485BusBase& bus, const int startIndex) const {
  return preValues.isSet(bus[startIndex + lookAhead]);
}

bool FilterByValue::postFilter(const RS485BusBase& bus, const int startIndex, const int endIndex) const {
  if(endIndex < startIndex + lookAhead) {
    return false;
  }
  return postValues.isSet(bus[startIndex + lookAhead]);
}

ValueFilter::ValueFilter() {
  this->rejectAll();
}

void ValueFilter::allowAll() {
  memset(values, 0xff, sizeof(values) / sizeof(values[0]));
}

void ValueFilter::rejectAll() {
  memset(values, 0, sizeof(values) / sizeof(values[0]));
}

void ValueFilter::allow(uint8_t byte) {
  this->set(byte);
}

void ValueFilter::reject(uint8_t byte) {
  this->clear(byte);
}

void ValueFilter::set(uint8_t byte) {
  uint8_t lookAhead = byte >> 3;
  uint8_t shift = byte & 0x7;
  values[lookAhead] |= 1 << shift;
}

void ValueFilter::clear(uint8_t byte) {
  uint8_t lookAhead = byte >> 3;
  uint8_t shift = byte & 0x7;
  values[lookAhead] &= ~(1 << shift);
}

bool ValueFilter::isSet(uint8_t byte) const {
  uint8_t lookAhead = byte >> 3;
  uint8_t shift = byte & 0x7;
  return (values[lookAhead] & (1 << shift)) != 0;
}