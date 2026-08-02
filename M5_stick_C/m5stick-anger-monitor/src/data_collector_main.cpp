// Standalone firmware for building an Edge Impulse training set: connects
// to WiFi, then lets BtnB pick a label and BtnA start/stop a recording,
// uploading the clip to Edge Impulse's ingestion API on stop. Flash this
// env temporarily for data-collection sessions, then flash the normal
// "m5stick-c" env back for day-to-day anger monitoring — the two don't run
// together, see platformio.ini.
#include <M5Unified.h>

#include "Config.h"
#include "EdgeImpulseUploader.h"

namespace {

constexpr const char* kLabels[] = {"angry", "calm"};
constexpr int kLabelCount = sizeof(kLabels) / sizeof(kLabels[0]);
int labelIndex = 0;

int16_t audioBuffer[kAudioFrameSamples];
EdgeImpulseUploader uploader(kSampleRateHz, kDataCollectMaxSamples);

void renderStatus() {
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setCursor(4, 4);
  M5.Lcd.setTextSize(2);
  M5.Lcd.printf("Label: %s", kLabels[labelIndex]);

  M5.Lcd.setTextSize(1);
  if (uploader.isRecording()) {
    float sec = uploader.recordedSamples() / static_cast<float>(kSampleRateHz);
    float maxSec =
        uploader.maxSamples() / static_cast<float>(kSampleRateHz);
    M5.Lcd.setCursor(4, 30);
    M5.Lcd.printf("REC %.1fs / %.1fs max", sec, maxSec);
    M5.Lcd.setCursor(4, 46);
    M5.Lcd.print("BtnA: stop + upload");
  } else {
    M5.Lcd.setCursor(4, 30);
    M5.Lcd.print("BtnB: change label");
    M5.Lcd.setCursor(4, 46);
    M5.Lcd.print("BtnA: start recording");
  }
}

void showResult(bool ok) {
  M5.Lcd.fillScreen(ok ? GREEN : RED);
  M5.Lcd.setTextColor(BLACK);
  M5.Lcd.setCursor(4, 4);
  M5.Lcd.setTextSize(2);
  M5.Lcd.print(ok ? "Uploaded!" : "Upload failed");
  M5.Lcd.setTextColor(WHITE);
  delay(1200);
}

}  // namespace

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);

  M5.Lcd.setRotation(1);
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setCursor(4, 4);
  M5.Lcd.print("Connecting WiFi...");

  if (!uploader.connectWifi()) {
    M5.Lcd.fillScreen(RED);
    M5.Lcd.setCursor(4, 4);
    M5.Lcd.print("WiFi failed.\nCheck Secrets.h,\nthen reset.");
    while (true) delay(1000);
  }

  M5.Mic.begin();
  renderStatus();
}

void loop() {
  M5.update();

  if (!uploader.isRecording() && M5.BtnB.wasPressed()) {
    labelIndex = (labelIndex + 1) % kLabelCount;
    renderStatus();
  }

  if (M5.BtnA.wasPressed()) {
    if (uploader.isRecording()) {
      M5.Lcd.fillScreen(BLACK);
      M5.Lcd.setCursor(4, 4);
      M5.Lcd.print("Uploading...");
      showResult(uploader.stopAndUpload());
    } else {
      uploader.startRecording(kLabels[labelIndex]);
    }
    renderStatus();
  }

  if (uploader.isRecording() && M5.Mic.isEnabled()) {
    if (M5.Mic.record(audioBuffer, kAudioFrameSamples, kSampleRateHz)) {
      uploader.feed(audioBuffer, kAudioFrameSamples);
      if (!uploader.isRecording()) {
        // Buffer filled up on its own — auto-upload so the clip isn't lost.
        showResult(uploader.stopAndUpload());
      }
      renderStatus();
    }
  }
}
