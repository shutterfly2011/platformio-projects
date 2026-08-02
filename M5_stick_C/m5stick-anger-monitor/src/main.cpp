#include <M5Unified.h>

#include "AngerMonitor.h"
#include "BeepAlert.h"
#include "Config.h"
#include "EnsembleAngerDetector.h"
#include "WaveformDisplay.h"

static int16_t audioBuffer[kAudioFrameSamples];
static EnsembleAngerDetector detector(kSampleRateHz, kFusionMode);
static AngerMonitor monitor(detector, kFramesToConfirm, kMonitorWindow,
                             kCooldownMs);

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);

  Serial.begin(115200);

  // M5Unified auto-detects the mic hardware for the running board
  // (PDM on M5StickC/Plus2), so no manual I2S pin config is needed here.
  M5.Mic.begin();
  M5.Speaker.setVolume(255);

  monitor.setOnAngerConfirmed([]() {
    Serial.println("Anger detected -> beeping");
    M5.Lcd.fillScreen(RED);
    beepAlertAngry();
    M5.Lcd.fillScreen(BLACK);
  });

  M5.Lcd.setRotation(1);
  displayInit();
}

void loop() {
  M5.update();

  if (M5.Mic.isEnabled()) {
    if (M5.Mic.record(audioBuffer, kAudioFrameSamples, kSampleRateHz)) {
      monitor.update(audioBuffer, kAudioFrameSamples);

      DetectorMetrics metrics{
          detector.lastEnergy(),      detector.lastZcr(),
          detector.lastPitchHz(),     detector.lastEnergyScore(),
          detector.lastZcrScore(),    detector.lastPitchScore(),
          detector.lastScore(),
      };
      displayRender(audioBuffer, kAudioFrameSamples, metrics);
    }
  }
}
