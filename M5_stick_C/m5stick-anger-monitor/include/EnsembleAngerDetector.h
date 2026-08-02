#pragma once
#include "HeuristicAngerDetector.h"
#include "IAngerDetector.h"

#ifdef USE_EDGE_IMPULSE
#include "EdgeImpulseAngerDetector.h"
#endif

// How the heuristic and ML verdicts combine into one per-frame decision.
enum class FusionMode {
  kOr,   // either detector calling "angry" is enough — favors recall
  kAnd,  // both must agree — favors precision, fewer false alarms
};

// Runs the heuristic DSP detector and (once trained and enabled via
// -DUSE_EDGE_IMPULSE, see EdgeImpulseAngerDetector.h) the Edge Impulse ML
// detector on every frame side by side, then fuses their verdicts per
// kFusionMode. Without USE_EDGE_IMPULSE this degrades to the heuristic
// alone, so it's safe to wire in before a model exists.
class EnsembleAngerDetector : public IAngerDetector {
 public:
  explicit EnsembleAngerDetector(uint32_t sampleRate,
                                  FusionMode mode = FusionMode::kOr);

  bool isAngry(const int16_t* samples, size_t count) override;

  // Passthroughs so the display/tuning code can keep reading the
  // heuristic's feature breakdown regardless of what else is running.
  float lastEnergy() const { return _heuristic.lastEnergy(); }
  float lastZcr() const { return _heuristic.lastZcr(); }
  float lastPitchHz() const { return _heuristic.lastPitchHz(); }
  float lastScore() const { return _heuristic.lastScore(); }
  float lastEnergyScore() const { return _heuristic.lastEnergyScore(); }
  float lastZcrScore() const { return _heuristic.lastZcrScore(); }
  float lastPitchScore() const { return _heuristic.lastPitchScore(); }

  // Per-detector verdicts from the most recent frame, for logging/tuning.
  bool lastHeuristicVerdict() const { return _lastHeuristicVerdict; }
  bool lastMlVerdict() const { return _lastMlVerdict; }

 private:
  HeuristicAngerDetector _heuristic;
#ifdef USE_EDGE_IMPULSE
  EdgeImpulseAngerDetector _ml;
#endif
  FusionMode _mode;
  bool _lastHeuristicVerdict = false;
  bool _lastMlVerdict = false;
};
