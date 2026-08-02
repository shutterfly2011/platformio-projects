#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>

// ---------------------------------------------------------------------------
// 40 kHz ultrasonic transmitter tester
//
// Drives a 16mm 40kHz piezo transmitter with a 40kHz square-wave carrier
// whose amplitude is modulated (AM) by a sine envelope. The envelope's own
// frequency is slowly swept between ENV_FREQ_MIN_HZ and ENV_FREQ_MAX_HZ, so
// on a scope (after a matching receiver) you should see the 40kHz carrier's
// amplitude "breathing" at a rate that itself rises and falls over several
// seconds.
//
// Wiring: connect CARRIER_PIN through a small series resistor (~100 ohm) to
// one leg of the transmitter, other leg to GND. Direct 3.3V GPIO drive is
// enough to verify a transmitter is alive with a nearby receiver + scope; add
// an external driver (MOSFET half-bridge / transistor buffer) if you need
// more range or output level.
//
// Caveat: a 16mm 40kHz transducer is a narrowband resonator (typically only
// a few kHz wide). AM sidebands sit at carrier +/- envelope frequency, so as
// the envelope frequency approaches 20kHz the sidebands (20kHz / 60kHz) fall
// outside what the transducer can pass mechanically - the modulation will
// look correct here in the drive signal, but may appear attenuated on the
// receiver/scope at the high end of the sweep. That's a physical limit of
// the transducer, not a firmware bug.
// ---------------------------------------------------------------------------

// ----- Carrier / modulation parameters --------------------------------------
static const uint8_t  CARRIER_PIN      = A0;    // 40kHz drive output, through series R to transmitter
static const uint32_t CARRIER_FREQ_HZ  = 40000; // transmitter resonant frequency
static const uint8_t  LEDC_CHANNEL     = 0;
static const uint8_t  LEDC_RESOLUTION_BITS = 8;  // duty 0-255

static const float ENV_FREQ_MIN_HZ = 1000.0f;
static const float ENV_FREQ_MAX_HZ = 20000.0f;
static const float SWEEP_HALF_PERIOD_S = 6.0f;  // seconds for 1kHz->20kHz (or back); full cycle = 2x this

// ----- DDS state (touched only by the carrier ISR) --------------------------
static uint8_t  sineLUT[256];
static volatile uint32_t envPhaseAccum   = 0;   // carrier-cycle sine phase, top 8 bits index sineLUT
static volatile uint32_t sweepPhaseAccum = 0;   // slow triangle sweep phase

static const float PHASE32_PER_HZ = 4294967296.0f / (float)CARRIER_FREQ_HZ;
static uint32_t sweepPhaseIncrement; // computed in setup() from SWEEP_HALF_PERIOD_S

hw_timer_t *carrierTimer = nullptr;

// ----- Shared state for the display (written by ISR, read by loop) ---------
volatile uint16_t g_envFreqHz = (uint16_t)ENV_FREQ_MIN_HZ;
volatile int8_t   g_sweepDir  = 1; // +1 rising, -1 falling

void IRAM_ATTR onCarrierTick() {
  sweepPhaseAccum += sweepPhaseIncrement;

  uint16_t triPos = sweepPhaseAccum >> 16; // 0..65535 across one full up+down cycle
  uint16_t triVal;
  int8_t dir;
  if (triPos < 32768) {
    triVal = triPos * 2;
    dir = 1;
  } else {
    triVal = 65535 - (triPos - 32768) * 2;
    dir = -1;
  }

  float triNorm = triVal / 65535.0f;
  float fInst = ENV_FREQ_MIN_HZ + (ENV_FREQ_MAX_HZ - ENV_FREQ_MIN_HZ) * triNorm;

  g_envFreqHz = (uint16_t)(fInst + 0.5f);
  g_sweepDir = dir;

  envPhaseAccum += (uint32_t)(fInst * PHASE32_PER_HZ);
  uint8_t duty = sineLUT[envPhaseAccum >> 24];

  ledcWrite(LEDC_CHANNEL, duty);
}

// ----- Display --------------------------------------------------------------
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

static const int16_t SCREEN_W = 240;
static const int16_t SCREEN_H = 135;

static const int16_t GRAPH_X = 0;
static const int16_t GRAPH_Y = 24;
static const int16_t GRAPH_W = SCREEN_W;
static const int16_t GRAPH_H = 76;

static const int16_t WAVE_Y = GRAPH_Y + GRAPH_H + 4;
static const int16_t WAVE_H = SCREEN_H - WAVE_Y;

static uint8_t freqHistory[GRAPH_W]; // column -> pixel row within graph, scrolls left
static uint16_t lastEnvFreq = 0;

static int16_t freqToY(uint16_t freqHz) {
  float norm = (freqHz - ENV_FREQ_MIN_HZ) / (ENV_FREQ_MAX_HZ - ENV_FREQ_MIN_HZ);
  if (norm < 0) norm = 0;
  if (norm > 1) norm = 1;
  return GRAPH_Y + GRAPH_H - 1 - (int16_t)(norm * (GRAPH_H - 1));
}

void initDisplay() {
  pinMode(TFT_I2C_POWER, OUTPUT);
  digitalWrite(TFT_I2C_POWER, HIGH);
  pinMode(TFT_BACKLITE, OUTPUT);
  digitalWrite(TFT_BACKLITE, HIGH);

  tft.init(135, 240);
  tft.setRotation(3);
  tft.fillScreen(ST77XX_BLACK);

  for (int16_t x = 0; x < GRAPH_W; x++) {
    freqHistory[x] = freqToY((uint16_t)ENV_FREQ_MIN_HZ);
  }

  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.setCursor(2, 2);
  tft.print("Carrier: 40.0 kHz   AM envelope:");
}

void drawSweepGraph(uint16_t envFreqHz) {
  // scroll left one column, append newest sample on the right
  memmove(freqHistory, freqHistory + 1, GRAPH_W - 1);
  freqHistory[GRAPH_W - 1] = freqToY(envFreqHz);

  tft.fillRect(GRAPH_X, GRAPH_Y, GRAPH_W, GRAPH_H, ST77XX_BLACK);
  for (int16_t x = 1; x < GRAPH_W; x++) {
    tft.drawLine(x - 1, freqHistory[x - 1], x, freqHistory[x], ST77XX_GREEN);
  }
  tft.drawFastHLine(GRAPH_X, GRAPH_Y, GRAPH_W, ST77XX_BLUE);
  tft.drawFastHLine(GRAPH_X, GRAPH_Y + GRAPH_H - 1, GRAPH_W, ST77XX_BLUE);
}

void drawEnvelopeSnapshot(uint16_t envFreqHz) {
  tft.fillRect(0, WAVE_Y, SCREEN_W, WAVE_H, ST77XX_BLACK);
  int16_t midY = WAVE_Y + WAVE_H / 2;
  float cycles = 3.0f; // always show a few periods regardless of instantaneous frequency
  int16_t prevY = midY;
  for (int16_t x = 0; x < SCREEN_W; x++) {
    float phase = (2.0f * PI * cycles * x) / SCREEN_W;
    int16_t y = midY - (int16_t)(sinf(phase) * (WAVE_H / 2 - 1));
    if (x > 0) tft.drawLine(x - 1, prevY, x, y, ST77XX_YELLOW);
    prevY = y;
  }
  (void)envFreqHz;
}

void updateReadout(uint16_t envFreqHz, int8_t sweepDir) {
  tft.fillRect(0, 12, SCREEN_W, 10, ST77XX_BLACK);
  tft.setCursor(2, 12);
  tft.setTextColor(ST77XX_WHITE);
  tft.printf("Env: %5u Hz  %s", envFreqHz, sweepDir > 0 ? "UP  " : "DOWN");
}

void setup() {
  Serial.begin(115200);

  for (int i = 0; i < 256; i++) {
    sineLUT[i] = (uint8_t)(127.5f + 127.5f * sinf(2.0f * PI * i / 256.0f));
  }
  sweepPhaseIncrement = (uint32_t)(4294967296.0 / (2.0 * SWEEP_HALF_PERIOD_S * CARRIER_FREQ_HZ));

  initDisplay();

  ledcSetup(LEDC_CHANNEL, CARRIER_FREQ_HZ, LEDC_RESOLUTION_BITS);
  ledcAttachPin(CARRIER_PIN, LEDC_CHANNEL);
  ledcWrite(LEDC_CHANNEL, 127);

  // 80MHz APB / 80 = 1MHz timer tick; alarm every 25 ticks = 25us = 40kHz,
  // exactly matching the carrier so duty updates once per carrier cycle.
  carrierTimer = timerBegin(0, 80, true);
  timerAttachInterrupt(carrierTimer, &onCarrierTick, true);
  timerAlarmWrite(carrierTimer, 25, true);
  timerAlarmEnable(carrierTimer);
}

void loop() {
  static uint32_t lastDrawMs = 0;
  uint32_t now = millis();
  if (now - lastDrawMs < 50) return; // ~20fps display refresh
  lastDrawMs = now;

  noInterrupts();
  uint16_t envFreqHz = g_envFreqHz;
  int8_t sweepDir = g_sweepDir;
  interrupts();

  if (envFreqHz != lastEnvFreq) {
    updateReadout(envFreqHz, sweepDir);
    lastEnvFreq = envFreqHz;
  }
  drawSweepGraph(envFreqHz);
  drawEnvelopeSnapshot(envFreqHz);
}
