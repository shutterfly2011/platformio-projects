#pragma once
#include <cstddef>
#include <cstdint>

// Common interface for anything that can look at one frame of audio and
// decide whether it sounds "angry" — whether that's a hand-tuned DSP
// heuristic (HeuristicAngerDetector) or a trained model (e.g. an Edge
// Impulse export, see EdgeImpulseAngerDetector.h). AngerMonitor only ever
// talks to this interface, so swapping the detector never touches the
// debounce/cooldown logic or the beep alert.
class IAngerDetector {
 public:
  virtual ~IAngerDetector() = default;

  // samples: mono 16-bit PCM audio frame
  // count:   number of samples in the frame
  // returns: true if this single frame looks "angry"
  virtual bool isAngry(const int16_t* samples, size_t count) = 0;
};
