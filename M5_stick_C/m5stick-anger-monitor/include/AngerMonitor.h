#pragma once
#include <functional>

#include "IAngerDetector.h"

// Debounces per-frame IAngerDetector output over time so a single loud
// syllable or noise burst doesn't trigger an alert. Requires at least
// framesToConfirm "angry" frames within the last windowSize frames, then
// enforces a cooldown before it will fire again.
class AngerMonitor {
 public:
  AngerMonitor(IAngerDetector& detector, uint8_t framesToConfirm,
               uint8_t windowSize, uint32_t cooldownMs);

  // Call once per captured audio frame.
  void update(const int16_t* samples, size_t count);

  void setOnAngerConfirmed(std::function<void()> cb) {
    _onAngerConfirmed = cb;
  }

 private:
  IAngerDetector& _detector;
  uint8_t _framesToConfirm;
  uint8_t _windowSize;
  uint32_t _cooldownMs;
  uint32_t _recentMask = 0;  // bit history of the last _windowSize frame results
  uint32_t _lastTriggerMs = 0;
  std::function<void()> _onAngerConfirmed;
};
