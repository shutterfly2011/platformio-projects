// M5StickC Plus2 audio streaming client.
//
// Press BtnA to start/stop streaming microphone audio to the configured
// server over a plain TCP socket. Wire protocol (must match server/recorder.py):
//   1. connect to SERVER_HOST:SERVER_PORT
//   2. send one line of JSON + '\n':
//        {"device_id":"...","sample_rate":16000,"bits":16,"channels":1}
//   3. stream raw little-endian PCM16 mono audio bytes continuously
//   4. close the socket to signal end of recording
//
// The device does not timestamp the header - the server uses its own
// receive/close time for the recording's start/end filename, which avoids
// needing NTP time sync on the device.

#include <M5Unified.h>
#include <WiFi.h>

#include "config.h"

enum class AppState { IDLE, RECORDING, ERROR };

static AppState state = AppState::IDLE;
static WiFiClient client;
static uint32_t recording_start_ms = 0;
static uint32_t last_display_update = 0;
static String error_message;
static uint32_t error_shown_at = 0;

static constexpr size_t CHUNK_SAMPLES = 512;
static int16_t audio_buffer[CHUNK_SAMPLES];

static constexpr uint32_t DISPLAY_INTERVAL_MS = 400;
static constexpr uint32_t ERROR_DISPLAY_MS = 3000;
static constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000;
static constexpr uint32_t TCP_CONNECT_TIMEOUT_MS = 3000;

static void drawScreen();

static void showError(const String &message) {
    error_message = message;
    error_shown_at = millis();
    state = AppState::ERROR;
    drawScreen();
}

static bool ensureWiFiConnected() {
    if (WiFi.status() == WL_CONNECTED) {
        return true;
    }
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
        delay(200);
        M5.update();
    }
    return WiFi.status() == WL_CONNECTED;
}

static void startRecording() {
    if (!ensureWiFiConnected()) {
        showError("WiFi connect failed");
        return;
    }

    client.setTimeout(TCP_CONNECT_TIMEOUT_MS / 1000 + 1);
    if (!client.connect(SERVER_HOST, SERVER_PORT, TCP_CONNECT_TIMEOUT_MS)) {
        showError("Server connect failed");
        return;
    }

    String header = String("{\"device_id\":\"") + DEVICE_ID +
                     "\",\"sample_rate\":" + String(SAMPLE_RATE) +
                     ",\"bits\":16,\"channels\":1}\n";
    client.print(header);

    auto mic_cfg = M5.Mic.config();
    mic_cfg.sample_rate = SAMPLE_RATE;
    mic_cfg.stereo = false;
    M5.Mic.config(mic_cfg);
    if (!M5.Mic.begin()) {
        client.stop();
        showError("Mic init failed");
        return;
    }

    state = AppState::RECORDING;
    recording_start_ms = millis();
    drawScreen();
}

static void stopRecording(const char *reason = nullptr) {
    M5.Mic.end();
    if (client.connected()) {
        client.stop();
    }
    if (reason != nullptr) {
        showError(reason);
    } else {
        state = AppState::IDLE;
        drawScreen();
    }
}

static void streamAudioChunk() {
    if (!M5.Mic.record(audio_buffer, CHUNK_SAMPLES, SAMPLE_RATE)) {
        return;
    }
    if (!client.connected()) {
        stopRecording("Connection lost");
        return;
    }
    const size_t bytes = CHUNK_SAMPLES * sizeof(int16_t);
    const size_t written = client.write(reinterpret_cast<const uint8_t *>(audio_buffer), bytes);
    if (written != bytes) {
        stopRecording("Send failed");
    }
}

static void drawScreen() {
    auto &d = M5.Display;
    d.fillScreen(TFT_BLACK);
    d.setTextColor(TFT_WHITE, TFT_BLACK);

    d.setCursor(4, 4);
    d.setTextSize(2);
    d.println("Audio Recorder");

    d.setTextSize(1);
    d.setCursor(4, 24);
    if (WiFi.status() == WL_CONNECTED) {
        d.setTextColor(TFT_GREEN, TFT_BLACK);
        d.printf("WiFi: %s\n", WiFi.localIP().toString().c_str());
    } else {
        d.setTextColor(TFT_RED, TFT_BLACK);
        d.println("WiFi: not connected");
    }

    d.setTextColor(TFT_WHITE, TFT_BLACK);
    d.setCursor(4, 38);
    int32_t battery = M5.Power.getBatteryLevel();
    bool charging = M5.Power.isCharging() == m5::Power_Class::is_charging;
    d.printf("Battery: %ld%%%s\n", static_cast<long>(battery), charging ? " (chg)" : "");

    d.setCursor(4, 58);
    d.setTextSize(2);
    switch (state) {
        case AppState::IDLE:
            d.setTextColor(TFT_WHITE, TFT_BLACK);
            d.println("Ready");
            break;
        case AppState::RECORDING: {
            d.setTextColor(TFT_RED, TFT_BLACK);
            uint32_t elapsed_s = (millis() - recording_start_ms) / 1000;
            d.printf("REC %02lu:%02lu\n", static_cast<unsigned long>(elapsed_s / 60),
                      static_cast<unsigned long>(elapsed_s % 60));
            break;
        }
        case AppState::ERROR:
            d.setTextColor(TFT_YELLOW, TFT_BLACK);
            d.println(error_message);
            break;
    }

    d.setTextColor(TFT_WHITE, TFT_BLACK);
    d.setTextSize(1);
    d.setCursor(4, 120);
    d.println(state == AppState::RECORDING ? "BtnA: Stop" : "BtnA: Start/Stop Rec");
}

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);
    M5.Display.setRotation(1);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    drawScreen();
}

void loop() {
    M5.update();

    if (state == AppState::ERROR && millis() - error_shown_at > ERROR_DISPLAY_MS) {
        state = AppState::IDLE;
    }

    if (M5.BtnA.wasPressed()) {
        if (state == AppState::RECORDING) {
            stopRecording();
        } else {
            startRecording();
        }
    }

    if (state == AppState::RECORDING) {
        streamAudioChunk();
    }

    if (millis() - last_display_update >= DISPLAY_INTERVAL_MS) {
        drawScreen();
        last_display_update = millis();
    }
}
