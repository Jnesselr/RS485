#pragma once

#include "rs485/filter.h"

enum class ComboFilterType {
  BOTH_FILTERS_MUST_BE_VALID,   // Both filters must be valid for this filter to be considered valid.
  EITHER_FILTER_MUST_BE_VALID,  // Either one filter or the other must be valid
  LEFT_IGNORED,                 // Act as the right filter only
  RIGHT_IGNORED                 // Act as the left filter only
};

/**
 * This filter combines two or more filters into one. A ComboFilter can accept another combo filter for chaining
 * in a tree like structure. By default, both filters must be valid for both preFilter and postFilter, but you
 * can change this behavior using the preFilterType and postFilterType on this class. No guarantees are made about
 * calling order of the filters.
 *
 * This class will not free the filters passed in.
 *
 * Even when using LEFT_IGNORED or RIGHT_IGNORED for the filter type, the lookAheadBytes will still be the max of
 * the two input filters' lookAheadBytes.
 */
class ComboFilter: public Filter {
public:
  ComboFilter(const Filter* left, const Filter* right):
    left(left),
    right(right)
  {}

  // lookAheadBytes is the larger of the two filters' look ahead bytes.
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