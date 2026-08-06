#pragma once
#include <cstddef>
#include <cstdint>

#include "EnsembleAngerDetector.h"

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

// --- Detector fusion (EnsembleAngerDetector) ---
// Only matters once -DUSE_EDGE_IMPULSE is enabled and a trained model is
// wired in — until then the ensemble just runs the heuristic. kOr fires if
// either detector flags a frame (favors catching real anger, more false
// positives); kAnd requires both to agree (favors fewer false alarms, more
// missed detections).
constexpr FusionMode kFusionMode = FusionMode::kOr;

// --- Debounce / state machine (AngerMonitor) ---
constexpr uint8_t kMonitorWindow = 8;    // look at the last N frames (~1 s @ 128 ms/frame)
constexpr uint8_t kFramesToConfirm = 5;  // need at least this many angry frames in the window
constexpr uint32_t kCooldownMs = 4000;   // don't re-trigger for this long after an alert

// --- Alert (BeepAlert) ---
constexpr int kAlertBeepCount = 3;
constexpr int kAlertBeepFreqHz = 4000;
constexpr int kAlertBeepDurationMs = 200;
constexpr int kAlertBeepGapMs = 120;

// --- Data collection (EdgeImpulseUploader, "datacollector" env only) ---
// Recording buffer is preallocated at this size, so keep it within what's
// left of the M5StickC's ~320 KB SRAM once WiFi is up (~150-200 KB free is
// typical) — 5s @ 16 kHz/16-bit mono is 160 KB.
constexpr uint32_t kDataCollectMaxDurationSec = 5;
constexpr size_t kDataCollectMaxSamples =
    kSampleRateHz * kDataCollectMaxDurationSec;
// BtnA's own click is picked up by the mic for a moment after the press —
// skip mic frames for this long after startRecording() so it doesn't end
// up in the clip.
constexpr uint32_t kDataCollectStartDelayMs = 300;
