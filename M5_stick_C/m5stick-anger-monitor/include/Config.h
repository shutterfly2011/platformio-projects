#pragma once
#include <cstddef>
#include <cstdint>

// --- Audio capture ---
constexpr uint32_t kSampleRateHz = 16000;
constexpr size_t kAudioFrameSamples = 2048;  // ~128 ms per frame @ 16 kHz

// --- Heuristic feature thresholds ---
// These are starting points, not physics — tune them by watching
// lastEnergy()/lastZcr()/lastPitchHz()/lastScore() over Serial while you
// talk normally, then while you raise your voice, in your actual room.
constexpr float kEnergyGate = 800.0f;             // below this: treat as silence/background
constexpr float kEnergyAngryThreshold = 6000.0f;  // loudness considered "shouting"
constexpr float kPitchBaselineAlpha = 0.02f;      // EMA smoothing rate for the pitch baseline
constexpr float kPitchRaiseHz = 60.0f;            // pitch rise over baseline = "raised voice"
constexpr float kZcrAngryThreshold = 0.18f;       // zero-crossing rate for harsh/noisy voicing
constexpr float kScoreAngryThreshold = 0.6f;      // combined weighted score to call a frame "angry"
constexpr float kWeightEnergy = 0.5f;
constexpr float kWeightPitch = 0.3f;
constexpr float kWeightZcr = 0.2f;

// --- Debounce / state machine (AngerMonitor) ---
constexpr uint8_t kMonitorWindow = 8;    // look at the last N frames (~1 s @ 128 ms/frame)
constexpr uint8_t kFramesToConfirm = 5;  // need at least this many angry frames in the window
constexpr uint32_t kCooldownMs = 4000;   // don't re-trigger for this long after an alert

// --- Alert (BeepAlert) ---
constexpr int kAlertBeepCount = 3;
constexpr int kAlertBeepFreqHz = 4000;
constexpr int kAlertBeepDurationMs = 200;
constexpr int kAlertBeepGapMs = 120;
