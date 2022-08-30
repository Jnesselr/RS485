#include "rs485/filters/combo_filter.h"

unsigned int ComboFilter::lookAheadBytes() const {
  size_t leftLookAhead = left->lookAheadBytes();
  size_t rightLookAhead = right->lookAheadBytes();
  return leftLookAhead > rightLookAhead ? leftLookAhead : rightLookAhead;
};

bool ComboFilter::preFilter(const RS485BusBase& bus, const int startIndex) const {
  switch(preFilterType) {
    case ComboFilterType::BOTH_FILTERS_MUST_BE_VALID:
      if(! left->isEnabled()) return false;
      if(! right->isEnabled()) return false;
      return left->preFilter(bus, startIndex) && right->preFilter(bus, startIndex);
    case ComboFilterType::EITHER_FILTER_MUST_BE_VALID:
      if(left->isEnabled() && left->preFilter(bus, startIndex)) return true;
      if(right->isEnabled() && right->preFilter(bus, startIndex)) return true;
      return false;
    case ComboFilterType::LEFT_IGNORED:
      return right->isEnabled() && right->preFilter(bus, startIndex);
    case ComboFilterType::RIGHT_IGNORED:
      return left->isEnabled() && left->preFilter(bus, startIndex);
    default:
      return false;
  }
};

bool ComboFilter::postFilter(const RS485BusBase& bus, const int startIndex, const int endIndex) const {
  switch(postFilterType) {
    case ComboFilterType::BOTH_FILTERS_MUST_BE_VALID:
      if(! left->isEnabled()) return false;
      if(! right->isEnabled()) return false;
      return left->preFilter(bus, startIndex) && right->preFilter(bus, startIndex);
    case ComboFilterType::EITHER_FILTER_MUST_BE_VALID:
      if(left->isEnabled() && left->preFilter(bus, startIndex)) return true;
      if(right->isEnabled() && right->preFilter(bus, startIndex)) return true;
      return false;
    case ComboFilterType::LEFT_IGNORED:
      return right->isEnabled() && right->preFilter(bus, startIndex);
    case ComboFilterType::RIGHT_IGNORED:
      return left->isEnabled() && left->preFilter(bus, startIndex);
    default:
      return false;
  }
};

bool ComboFilter::isEnabled() const {
  if(! enabled) return false;
  return left->isEnabled() || right->isEnabled();
}