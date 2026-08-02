#include "EdgeImpulseUploader.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include <algorithm>
#include <cstring>

#include "Secrets.h"

namespace {

constexpr const char* kIngestionHost = "ingestion.edgeimpulse.com";
constexpr const char* kIngestionPath = "/api/training/data";
constexpr uint32_t kWifiConnectTimeoutMs = 15000;

void writeWavHeader(uint8_t* header, uint32_t dataBytes,
                     uint32_t sampleRate) {
  uint32_t byteRate = sampleRate * 2;  // mono, 16-bit
  uint32_t riffSize = 36 + dataBytes;

  memcpy(header, "RIFF", 4);
  header[4] = riffSize & 0xff;
  header[5] = (riffSize >> 8) & 0xff;
  header[6] = (riffSize >> 16) & 0xff;
  header[7] = (riffSize >> 24) & 0xff;
  memcpy(header + 8, "WAVE", 4);

  memcpy(header + 12, "fmt ", 4);
  header[16] = 16;
  header[17] = 0;
  header[18] = 0;
  header[19] = 0;  // fmt chunk size
  header[20] = 1;
  header[21] = 0;  // audio format = PCM
  header[22] = 1;
  header[23] = 0;  // mono
  header[24] = sampleRate & 0xff;
  header[25] = (sampleRate >> 8) & 0xff;
  header[26] = (sampleRate >> 16) & 0xff;
  header[27] = (sampleRate >> 24) & 0xff;
  header[28] = byteRate & 0xff;
  header[29] = (byteRate >> 8) & 0xff;
  header[30] = (byteRate >> 16) & 0xff;
  header[31] = (byteRate >> 24) & 0xff;
  header[32] = 2;
  header[33] = 0;  // block align
  header[34] = 16;
  header[35] = 0;  // bits per sample

  memcpy(header + 36, "data", 4);
  header[40] = dataBytes & 0xff;
  header[41] = (dataBytes >> 8) & 0xff;
  header[42] = (dataBytes >> 16) & 0xff;
  header[43] = (dataBytes >> 24) & 0xff;
}

}  // namespace

EdgeImpulseUploader::EdgeImpulseUploader(uint32_t sampleRate,
                                          size_t maxSamples)
    : _sampleRate(sampleRate),
      _maxSamples(maxSamples),
      _buffer(new int16_t[maxSamples]) {}

EdgeImpulseUploader::~EdgeImpulseUploader() { delete[] _buffer; }

bool EdgeImpulseUploader::connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(kWifiSsid, kWifiPassword);
  Serial.printf("Connecting to WiFi \"%s\"...\n", kWifiSsid);

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > kWifiConnectTimeoutMs) {
      Serial.println("WiFi connect timed out");
      return false;
    }
    delay(250);
  }
  Serial.print("WiFi connected, IP=");
  Serial.println(WiFi.localIP());
  return true;
}

void EdgeImpulseUploader::startRecording(const char* label) {
  _label = label;
  _writeIndex = 0;
  _recording = true;
}

void EdgeImpulseUploader::feed(const int16_t* samples, size_t count) {
  if (!_recording) return;

  size_t room = _maxSamples - _writeIndex;
  size_t toCopy = std::min(room, count);
  memcpy(_buffer + _writeIndex, samples, toCopy * sizeof(int16_t));
  _writeIndex += toCopy;

  if (_writeIndex >= _maxSamples) {
    _recording = false;  // buffer full — caller should upload what we have
  }
}

bool EdgeImpulseUploader::stopAndUpload() {
  _recording = false;
  if (_writeIndex == 0) return false;

  uint32_t dataBytes = static_cast<uint32_t>(_writeIndex * sizeof(int16_t));
  uint32_t totalBytes = 44 + dataBytes;
  uint8_t* wav = new uint8_t[totalBytes];
  writeWavHeader(wav, dataBytes, _sampleRate);
  memcpy(wav + 44, _buffer, dataBytes);

  WiFiClientSecure client;
  // Skips certificate validation rather than pinning Edge Impulse's root
  // CA, to avoid the firmware breaking silently on the next cert rotation.
  // Fine for a hobby data-collection tool; pin a CA if that risk matters
  // to you.
  client.setInsecure();

  HTTPClient http;
  String url = String("https://") + kIngestionHost + kIngestionPath;
  http.begin(client, url);
  http.addHeader("x-api-key", kEdgeImpulseApiKey);
  http.addHeader("x-label", _label);
  http.addHeader("x-file-name", String(_label) + ".wav");
  http.addHeader("Content-Type", "audio/wav");

  int status = http.POST(wav, totalBytes);
  Serial.printf("Edge Impulse upload: HTTP %d, %u bytes (%.1fs), label=%s\n",
                status, totalBytes, _writeIndex / static_cast<float>(_sampleRate),
                _label);
  http.end();
  delete[] wav;

  _writeIndex = 0;
  return status >= 200 && status < 300;
}
