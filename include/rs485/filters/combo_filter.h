#pragma once

#include "rs485/filter.h"
#include <inttypes.h>

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

  virtual unsigned int lookAheadBytes() const;

  virtual bool preFilter(const RS485BusBase& bus, const int startIndex) const;

  virtual bool postFilter(const RS485BusBase& bus, const int startIndex, const int endIndex) const;

  virtual bool isEnabled() const;

  ComboFilterType preFilterType = ComboFilterType::BOTH_FILTERS_MUST_BE_VALID;
  ComboFilterType postFilterType = ComboFilterType::BOTH_FILTERS_MUST_BE_VALID;

private:
  const Filter* const left = nullptr;
  const Filter* const right = nullptr;
};