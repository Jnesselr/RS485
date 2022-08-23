#include "fixtures.h"

#include <ArduinoFake.h>

using namespace fakeit;

PrepBus::PrepBus() {
  ArduinoFakeReset();

  When(Method(ArduinoFake(), pinMode)).AlwaysReturn();
  When(Method(ArduinoFake(), digitalWrite)).AlwaysReturn();
  When(Method(ArduinoFake(), delay)).AlwaysReturn();
}