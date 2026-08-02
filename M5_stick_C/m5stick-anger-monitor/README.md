# M5Stick Anger Monitor

Listens to the M5StickC's built-in mic, scores each audio frame for
"angry-sounding" speech (loud, harsh, pitched-up), and beeps + flashes the
screen red when it's confident enough, over a sustained window, that you're
raising your voice.

## Architecture

Everything hangs off one interface, `IAngerDetector::isAngry(samples, count)`,
so the debounce logic and the detector implementation never depend on each
other:

- **`HeuristicAngerDetector`** — the only detector actually trained/tuned
  today. Computes RMS energy, zero-crossing rate, and a pitch estimate
  (autocorrelation) per frame, compares each against a threshold in
  `Config.h`, and combines them into a weighted score.
- **`EdgeImpulseAngerDetector`** — a stub for a TinyML model trained in Edge
  Impulse Studio (MFCC/MFE features + small NN). Compiles to nothing unless
  `-DUSE_EDGE_IMPULSE` is set — **no trained model is wired in yet**, so
  right now it always reports "not angry."
- **`EnsembleAngerDetector`** — runs both of the above per frame and fuses
  their verdicts (`kOr`: either fires; `kAnd`: both must agree). This is
  what `main.cpp` actually instantiates. With `USE_EDGE_IMPULSE` off (the
  current default), it's equivalent to the heuristic alone.
- **`AngerMonitor`** — debounces the per-frame verdicts: requires N "angry"
  frames within a sliding window before firing, then enforces a cooldown so
  one shout doesn't retrigger repeatedly.
- **`BeepAlert`** / **`WaveformDisplay`** — the speaker alert and the
  on-screen waveform + per-metric bar gauges (used for tuning thresholds by
  eye/Serial while you talk).

## PlatformIO environments

- **`m5stick-c`** — the monitor firmware above (`src/main.cpp`). No WiFi,
  no network dependency.
- **`datacollector`** — a separate standalone firmware
  (`src/data_collector_main.cpp`) for building an Edge Impulse training
  set: connects to WiFi, lets you pick a label and start/stop a recording
  with the buttons, and uploads the clip to Edge Impulse's ingestion API on
  stop. The two environments are mutually exclusive on the device — flash
  one, use it, reflash the other.

```sh
pio run -e m5stick-c -t upload         # flash the monitor
pio run -e datacollector -t upload     # flash the data-collection tool
```

## Current status

- ✅ Heuristic detector + debounce + beep alert + waveform display: built,
  flashes, and boots cleanly on hardware (confirmed via serial boot log).
  Threshold tuning (`Config.h`) for your voice/room has **not** been done
  yet — the shipped values are starting points, not calibrated numbers.
- ✅ `datacollector` firmware: builds and flashes; WiFi + upload flow is
  implemented but **end-to-end upload to Edge Impulse is not yet
  verified** — needs `include/Secrets.h` filled in with real WiFi
  credentials and an Edge Impulse API key (copy from
  `Secrets.h.example`, gitignored).
- ⬜ No Edge Impulse model has been trained yet. `EdgeImpulseAngerDetector`
  is a stub; the ensemble currently runs heuristic-only.
- ⬜ No labeled training data collected yet.

## Next steps

1. Fill in `include/Secrets.h`, flash `datacollector`, and confirm clips
   actually land in the Edge Impulse project's **Data acquisition** tab.
2. Record a batch of "angry" and "calm" clips.
3. Train an audio classification model in Edge Impulse Studio, export it
   as an Arduino library, drop it in `lib_deps`, and fill in
   `EdgeImpulseAngerDetector::isAngry()`.
4. Add `-DUSE_EDGE_IMPULSE` to `platformio.ini`'s `build_flags` and tune
   `kFusionMode` in `Config.h` (OR vs AND) against real data from both
   detectors.
5. Tune the heuristic thresholds in `Config.h` against your actual
   voice/room using the on-screen bar gauges and `lastEnergy()`/`lastZcr()`/
   `lastPitchHz()`/`lastScore()` over Serial.
