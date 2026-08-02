#include "BeepAlert.h"

#include <M5Unified.h>

#include "Config.h"

void beepAlertAngry() {
  for (int i = 0; i < kAlertBeepCount; ++i) {
    M5.Speaker.tone(kAlertBeepFreqHz, kAlertBeepDurationMs);
    delay(kAlertBeepDurationMs + kAlertBeepGapMs);
  }
}
