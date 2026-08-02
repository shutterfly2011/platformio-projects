#include "EnsembleAngerDetector.h"

EnsembleAngerDetector::EnsembleAngerDetector(uint32_t sampleRate,
                                              FusionMode mode)
    : _heuristic(sampleRate),
#ifdef USE_EDGE_IMPULSE
      _ml(sampleRate),
#endif
      _mode(mode) {
}

bool EnsembleAngerDetector::isAngry(const int16_t* samples, size_t count) {
  _lastHeuristicVerdict = _heuristic.isAngry(samples, count);

#ifndef USE_EDGE_IMPULSE
  _lastMlVerdict = false;
  return _lastHeuristicVerdict;
#else
  _lastMlVerdict = _ml.isAngry(samples, count);

  if (_mode == FusionMode::kAnd) {
    return _lastHeuristicVerdict && _lastMlVerdict;
  }
  return _lastHeuristicVerdict || _lastMlVerdict;
#endif
}
