#pragma once
#include "IAngerDetector.h"

// Rule-based anger classifier using three acoustic cues extracted per
// frame: loudness (RMS energy), voice harshness (zero-crossing rate), and
// a raised pitch relative to a slow-moving per-speaker baseline. This is a
// proxy for the paralinguistic signature of anger (loud, harsh, pitched-up
// speech) — it does not understand words or meaning.
class HeuristicAngerDetector : public IAngerDetector {
 public:
  explicit HeuristicAngerDetector(uint32_t sampleRate);

  bool isAngry(const int16_t* samples, size_t count) override;

  // Exposed for tuning: print these over Serial while adjusting Config.h.
  float lastEnergy() const { return _lastEnergy; }
  float lastZcr() const { return _lastZcr; }
  float lastPitchHz() const { return _lastPitchHz; }
  float lastScore() const { return _lastScore; }

  // Each is (value / its threshold), clamped to 1.5 — the same normalized
  // form isAngry() weighs internally. A display can treat 1.0 as "at
  // threshold" for all three uniformly.
  float lastEnergyScore() const { return _lastEnergyScore; }
  float lastZcrScore() const { return _lastZcrScore; }
  float lastPitchScore() const { return _lastPitchScore; }

 private:
  float computeRmsEnergy(const int16_t* samples, size_t count) const;
  float computeZeroCrossingRate(const int16_t* samples, size_t count) const;
  float estimatePitchHz(const int16_t* samples, size_t count) const;

  uint32_t _sampleRate;
  float _lastEnergy = 0;
  float _lastZcr = 0;
  float _lastPitchHz = 0;
  float _lastScore = 0;
  float _lastEnergyScore = 0;
  float _lastZcrScore = 0;
  float _lastPitchScore = 0;
  float _pitchBaseline = 0;  // running EMA of "normal" pitch for this speaker/room
};
