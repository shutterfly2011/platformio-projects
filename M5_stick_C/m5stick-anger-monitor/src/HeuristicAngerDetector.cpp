#include "HeuristicAngerDetector.h"

#include <algorithm>
#include <cmath>

#include "Config.h"

HeuristicAngerDetector::HeuristicAngerDetector(uint32_t sampleRate)
    : _sampleRate(sampleRate) {}

float HeuristicAngerDetector::computeRmsEnergy(const int16_t* samples,
                                                size_t count) const {
  double sumSq = 0;
  for (size_t i = 0; i < count; ++i) {
    double s = samples[i];
    sumSq += s * s;
  }
  return static_cast<float>(std::sqrt(sumSq / count));
}

float HeuristicAngerDetector::computeZeroCrossingRate(const int16_t* samples,
                                                       size_t count) const {
  if (count < 2) return 0.0f;
  size_t crossings = 0;
  for (size_t i = 1; i < count; ++i) {
    if ((samples[i - 1] >= 0) != (samples[i] >= 0)) {
      ++crossings;
    }
  }
  return static_cast<float>(crossings) / static_cast<float>(count - 1);
}

float HeuristicAngerDetector::estimatePitchHz(const int16_t* samples,
                                               size_t count) const {
  // Autocorrelation restricted to the human voice F0 range (~80-400 Hz).
  const size_t minLag = _sampleRate / 400;
  const size_t maxLag = _sampleRate / 80;
  if (maxLag == 0 || maxLag >= count) return 0.0f;

  size_t bestLag = 0;
  double bestCorr = 0;
  for (size_t lag = minLag; lag <= maxLag; ++lag) {
    double corr = 0;
    size_t n = count - lag;
    for (size_t i = 0; i < n; ++i) {
      corr += static_cast<double>(samples[i]) *
              static_cast<double>(samples[i + lag]);
    }
    if (corr > bestCorr) {
      bestCorr = corr;
      bestLag = lag;
    }
  }
  if (bestLag == 0) return 0.0f;
  return static_cast<float>(_sampleRate) / static_cast<float>(bestLag);
}

bool HeuristicAngerDetector::isAngry(const int16_t* samples, size_t count) {
  float energy = computeRmsEnergy(samples, count);
  float zcr = computeZeroCrossingRate(samples, count);
  _lastEnergy = energy;
  _lastZcr = zcr;

  // Below the noise gate there's nothing meaningful to classify; don't let
  // silence drag the pitch baseline around either.
  if (energy < kEnergyGate) {
    _lastPitchHz = 0;
    _lastScore = 0;
    _lastEnergyScore = 0;
    _lastZcrScore = 0;
    _lastPitchScore = 0;
    return false;
  }

  float pitchHz = estimatePitchHz(samples, count);
  _lastPitchHz = pitchHz;

  if (pitchHz > 0) {
    if (_pitchBaseline <= 0) {
      _pitchBaseline = pitchHz;
    } else {
      _pitchBaseline += kPitchBaselineAlpha * (pitchHz - _pitchBaseline);
    }
  }

  float energyScore = std::min(energy / kEnergyAngryThreshold, 1.5f);
  float pitchRise =
      (pitchHz > 0 && _pitchBaseline > 0) ? (pitchHz - _pitchBaseline) : 0.0f;
  float pitchScore = std::min(std::max(pitchRise, 0.0f) / kPitchRaiseHz, 1.5f);
  float zcrScore = std::min(zcr / kZcrAngryThreshold, 1.5f);

  float score = kWeightEnergy * energyScore + kWeightPitch * pitchScore +
                kWeightZcr * zcrScore;
  _lastScore = score;
  _lastEnergyScore = energyScore;
  _lastZcrScore = zcrScore;
  _lastPitchScore = pitchScore;

  return score >= kScoreAngryThreshold;
}
