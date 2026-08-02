#include "WaveformDisplay.h"

#include <M5Unified.h>

#include <algorithm>

namespace {

M5Canvas canvas(&M5.Lcd);

constexpr int kScreenW = 240;
constexpr int kScreenH = 135;

constexpr int kWaveTop = 1;
constexpr int kWaveHeight = 40;
constexpr int kWaveCenterY = kWaveTop + kWaveHeight / 2;
constexpr int kWaveHalfRange = kWaveHeight / 2 - 2;

constexpr int kBarX = 10;
constexpr int kBarW = kScreenW - kBarX - 4;
constexpr int kBarH = 14;
constexpr int kRowH = 18;
constexpr int kFirstRowY = 44;

constexpr int kFootnoteY = 122;

// HeuristicAngerDetector clamps every component score to value/threshold,
// capped at 1.5 — so "1.0" always means "right at threshold" regardless of
// which metric it is, and one constant marks that point on every bar.
constexpr float kScoreDisplayMax = 1.5f;
constexpr float kThresholdFraction = 1.0f / kScoreDisplayMax;

void drawBar(int rowIndex, const char* label, float scoreValue) {
  int y = kFirstRowY + rowIndex * kRowH;
  float frac =
      std::min(std::max(scoreValue / kScoreDisplayMax, 0.0f), 1.0f);

  canvas.setTextColor(TFT_WHITE);
  canvas.setTextSize(1);
  canvas.setCursor(0, y + 3);
  canvas.print(label);

  canvas.drawRect(kBarX, y, kBarW, kBarH, TFT_DARKGREY);
  int fillW = static_cast<int>(frac * (kBarW - 2));
  uint16_t barColor = (scoreValue >= 1.0f) ? TFT_RED : TFT_GREEN;
  if (fillW > 0) {
    canvas.fillRect(kBarX + 1, y + 1, fillW, kBarH - 2, barColor);
  }

  int tickX = kBarX + static_cast<int>(kThresholdFraction * (kBarW - 2));
  canvas.drawFastVLine(tickX, y - 2, kBarH + 4, TFT_YELLOW);
}

}  // namespace

void displayInit() {
  canvas.setColorDepth(16);
  canvas.createSprite(kScreenW, kScreenH);
  canvas.setTextSize(1);
}

void displayRender(const int16_t* samples, size_t count,
                    const DetectorMetrics& metrics) {
  canvas.fillSprite(TFT_BLACK);

  // Waveform: min/max per pixel column (not simple decimation) so short
  // loud bursts within a column still show their full peak-to-peak swing.
  canvas.drawFastHLine(0, kWaveCenterY, kScreenW, TFT_NAVY);
  size_t samplesPerCol = std::max<size_t>(1, count / kScreenW);
  for (int x = 0; x < kScreenW; ++x) {
    size_t start = static_cast<size_t>(x) * samplesPerCol;
    if (start >= count) break;
    size_t end = std::min(start + samplesPerCol, count);

    int16_t lo = 32767;
    int16_t hi = -32768;
    for (size_t i = start; i < end; ++i) {
      lo = std::min(lo, samples[i]);
      hi = std::max(hi, samples[i]);
    }

    int yLo = kWaveCenterY - (static_cast<int>(hi) * kWaveHalfRange) / 32768;
    int yHi = kWaveCenterY - (static_cast<int>(lo) * kWaveHalfRange) / 32768;
    canvas.drawFastVLine(x, yLo, std::max(1, yHi - yLo + 1), TFT_CYAN);
  }

  drawBar(0, "E", metrics.energyScore);
  drawBar(1, "Z", metrics.zcrScore);
  drawBar(2, "P", metrics.pitchScore);
  drawBar(3, "S", metrics.combinedScore);

  canvas.setTextColor(TFT_LIGHTGREY);
  canvas.setTextSize(1);
  canvas.setCursor(4, kFootnoteY);
  canvas.print("Hold power button 6s to turn off");

  canvas.pushSprite(0, 0);
}
