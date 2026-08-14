#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>

// ---------------------------------------------------------------------------
// HC-SR04 ultrasonic radar
//
// Triggers the HC-SR04, times the echo pulse, and shows the resulting
// distance on the built-in TFT along with a simple range bar.
//
// Wiring:
//   HC-SR04 VCC  -> Feather 3V (most HC-SR04 modules work down to ~3.3V,
//                    at somewhat reduced range/reliability; use 5V/USB if
//                    you need full range and step ECHO down as below)
//   HC-SR04 GND  -> Feather GND
//   HC-SR04 TRIG -> TRIG_PIN directly (ESP32 3.3V output is a valid HIGH
//                    for the HC-SR04's trigger input)
//   HC-SR04 ECHO -> ECHO_PIN through a voltage divider (e.g. 1k from ECHO
//                    to ECHO_PIN, 2k from ECHO_PIN to GND) if VCC is 5V,
//                    since ECHO idles at VCC and the ESP32-S3's GPIOs are
//                    not 5V tolerant. Skip the divider only if running the
//                    sensor from 3.3V.
// ---------------------------------------------------------------------------

static const uint8_t TRIG_PIN = A0;
static const uint8_t ECHO_PIN = A1;

static const float SOUND_SPEED_CM_PER_US = 0.0343f;
static const float MAX_DISTANCE_CM = 400.0f;    // HC-SR04 datasheet max range
static const uint32_t ECHO_TIMEOUT_US = 30000;  // round trip for ~514cm, comfortably beyond max range
static const uint32_t MEASURE_INTERVAL_MS = 100; // HC-SR04 needs >=60ms between triggers to avoid echo cross-talk

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

static const int16_t SCREEN_W = 240;
static const int16_t SCREEN_H = 135;
static const int16_t BAR_X = 4;
static const int16_t BAR_Y = 100;
static const int16_t BAR_W = SCREEN_W - 2 * BAR_X;
static const int16_t BAR_H = 20;

// Returns distance in cm, or -1 if no echo was received (out of range / no target).
float measureDistanceCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  uint32_t durationUs = pulseIn(ECHO_PIN, HIGH, ECHO_TIMEOUT_US);
  if (durationUs == 0) {
    return -1.0f;
  }
  return (durationUs * SOUND_SPEED_CM_PER_US) / 2.0f;
}

void drawRangeBar(float distanceCm) {
  tft.fillRect(BAR_X, BAR_Y, BAR_W, BAR_H, ST77XX_BLACK);
  tft.drawRect(BAR_X, BAR_Y, BAR_W, BAR_H, ST77XX_BLUE);

  if (distanceCm < 0) return;

  float norm = distanceCm / MAX_DISTANCE_CM;
  if (norm > 1.0f) norm = 1.0f;
  int16_t fillW = (int16_t)((BAR_W - 2) * norm);

  uint16_t color = distanceCm < 10.0f ? ST77XX_RED
                  : distanceCm < 50.0f ? ST77XX_YELLOW
                  : ST77XX_GREEN;
  tft.fillRect(BAR_X + 1, BAR_Y + 1, fillW, BAR_H - 2, color);
}

void drawDistance(float distanceCm) {
  tft.fillRect(0, 30, SCREEN_W, 60, ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(4, 40);

  if (distanceCm < 0) {
    tft.setTextSize(3);
    tft.print("OUT OF RANGE");
  } else {
    tft.setTextSize(4);
    tft.printf("%.1f cm", distanceCm);
    tft.setTextSize(2);
    tft.setCursor(4, 76);
    tft.printf("%.1f in", distanceCm / 2.54f);
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(TRIG_PIN, OUTPUT);
  digitalWrite(TRIG_PIN, LOW);
  pinMode(ECHO_PIN, INPUT);

  pinMode(TFT_I2C_POWER, OUTPUT);
  digitalWrite(TFT_I2C_POWER, HIGH);
  pinMode(TFT_BACKLITE, OUTPUT);
  digitalWrite(TFT_BACKLITE, HIGH);

  tft.init(135, 240);
  tft.setRotation(3);
  tft.fillScreen(ST77XX_BLACK);

  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.setCursor(4, 4);
  tft.print("Ultrasonic Radar - HC-SR04");
}

void loop() {
  static uint32_t lastMeasureMs = 0;
  uint32_t now = millis();
  if (now - lastMeasureMs < MEASURE_INTERVAL_MS) return;
  lastMeasureMs = now;

  float distanceCm = measureDistanceCm();
  drawDistance(distanceCm);
  drawRangeBar(distanceCm);

  if (distanceCm < 0) {
    Serial.println("out of range");
  } else {
    Serial.printf("%.1f cm\n", distanceCm);
  }
}
