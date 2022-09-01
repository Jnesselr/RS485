#pragma once

#include "rs485/filter.h"

enum class ComboFilterType {
  BOTH_FILTERS_MUST_BE_VALID,
  EITHER_FILTER_MUST_BE_VALID,
  LEFT_IGNORED,
  RIGHT_IGNORED
};

class ComboFilter: public Filter {
public:
  ComboFilter(const Filter& left, const Filter& right):
    left(&left),
    right(&right)
  {}

  virtual size_t lookAheadBytes() const;

  virtual bool preFilter(const RS485BusBase& bus, const size_t startIndex) const;

  virtual bool postFilter(const RS485BusBase& bus, const size_t startIndex, const size_t endIndex) const;

  virtual bool isEnabled() const;

  ComboFilterType preFilterType = ComboFilterType::BOTH_FILTERS_MUST_BE_VALID;
  ComboFilterType postFilterType = ComboFilterType::BOTH_FILTERS_MUST_BE_VALID;

private:
  const Filter* const left = nullptr;
  const Filter* const right = nullptr;
};