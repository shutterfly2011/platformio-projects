#include "AngerMonitor.h"

#include <Arduino.h>  // millis()

AngerMonitor::AngerMonitor(IAngerDetector& detector, uint8_t framesToConfirm,
                            uint8_t windowSize, uint32_t cooldownMs)
    : _detector(detector),
      _framesToConfirm(framesToConfirm),
      _windowSize(windowSize),
      _cooldownMs(cooldownMs) {}

void AngerMonitor::update(const int16_t* samples, size_t count) {
  bool angryFrame = _detector.isAngry(samples, count);

  _recentMask <<= 1;
  if (angryFrame) _recentMask |= 1;
  _recentMask &= (1u << _windowSize) - 1;

  int angryCount = __builtin_popcount(_recentMask);

  uint32_t now = millis();
  bool cooldownElapsed = (now - _lastTriggerMs) >= _cooldownMs;

  if (angryCount >= _framesToConfirm && cooldownElapsed) {
    _lastTriggerMs = now;
    _recentMask = 0;  // require a fresh run of angry frames before the next alert
    if (_onAngerConfirmed) _onAngerConfirmed();
  }
}
