#pragma once
#include <cstddef>
#include <cstdint>

// Snapshot of one frame's metrics, decoupled from HeuristicAngerDetector so
// the display doesn't need to know about the detector class itself.
struct DetectorMetrics {
  float energy;
  float zcr;
  float pitchHz;
  // Each is (value / threshold), clamped to 1.5 by the detector — see
  // HeuristicAngerDetector::lastEnergyScore() etc. 1.0 means "at threshold".
  float energyScore;
  float zcrScore;
  float pitchScore;
  float combinedScore;
};

// Call once in setup(), after M5.begin() and M5.Lcd.setRotation(...).
void displayInit();

// Call once per audio frame: draws a min/max waveform trace plus bar
// gauges for energy/ZCR/pitch/combined score, each with a threshold tick.
void displayRender(const int16_t* samples, size_t count,
                    const DetectorMetrics& metrics);
