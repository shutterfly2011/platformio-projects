#pragma once
// Placeholder "plug" for a future TinyML upgrade. Not compiled unless you
// opt in — see the #ifdef below — so it has zero effect until you use it.
//
// To wire in a trained model later:
//   1. In Edge Impulse Studio, record labeled clips ("angry" vs.
//      "calm"/"neutral" speech) and train an audio classification model
//      (MFCC/MFE features + a small NN work well for this).
//   2. Deployment -> Arduino library -> download the generated .zip and
//      add it as a project dependency (e.g. drop it in lib/, or add it to
//      platformio.ini's lib_deps as a local/zip dependency).
//   3. Uncomment the #include below and point it at the generated
//      "<your-project>_inferencing.h" header.
//   4. Implement isAngry() to run the classifier on the raw frame and
//      return true when the "angry" label's confidence clears
//      kEdgeImpulseConfidenceThreshold.
//   5. In platformio.ini, add `-DUSE_EDGE_IMPULSE` to build_flags.
//   6. In main.cpp, swap `HeuristicAngerDetector detector(...)` for
//      `EdgeImpulseAngerDetector detector(...)`. AngerMonitor and
//      BeepAlert need no changes — both only depend on IAngerDetector.

#include "IAngerDetector.h"

#ifdef USE_EDGE_IMPULSE
// #include "your-edge-impulse-project_inferencing.h"

constexpr float kEdgeImpulseConfidenceThreshold = 0.8f;

class EdgeImpulseAngerDetector : public IAngerDetector {
 public:
  explicit EdgeImpulseAngerDetector(uint32_t sampleRate)
      : _sampleRate(sampleRate) {}

  bool isAngry(const int16_t* samples, size_t count) override {
    // signal_t signal;
    // signal.total_length = count;
    // signal.get_data = [samples](size_t offset, size_t length,
    //                              float* out_ptr) -> int {
    //   for (size_t i = 0; i < length; i++) out_ptr[i] = samples[offset + i];
    //   return 0;
    // };
    // ei_impulse_result_t result;
    // run_classifier(&signal, &result, false);
    // for (auto& c : result.classification) {
    //   if (String(c.label) == "angry" &&
    //       c.value > kEdgeImpulseConfidenceThreshold) {
    //     return true;
    //   }
    // }
    return false;
  }

 private:
  uint32_t _sampleRate;
};
#endif  // USE_EDGE_IMPULSE
