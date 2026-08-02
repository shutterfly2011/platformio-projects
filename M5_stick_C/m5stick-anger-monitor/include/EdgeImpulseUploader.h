#pragma once
#include <cstddef>
#include <cstdint>

// Buffers mic frames in RAM while "recording", then uploads the whole clip
// as one labeled WAV file to Edge Impulse's ingestion API when told to
// stop. Edge Impulse's ingestion API takes one complete file per call —
// there's no continuous/incremental ingestion endpoint — so "start/stop
// streaming" here means "buffer while recording, upload on stop."
class EdgeImpulseUploader {
 public:
  EdgeImpulseUploader(uint32_t sampleRate, size_t maxSamples);
  ~EdgeImpulseUploader();

  // Connects to WiFi using the credentials in Secrets.h. Call once from
  // setup(); blocks (with a timeout) until connected.
  bool connectWifi();

  void startRecording(const char* label);

  // Appends one mic frame to the in-progress recording. No-op if not
  // currently recording. Auto-stops (without uploading) once the buffer
  // fills — the caller should check isRecording() after and upload.
  void feed(const int16_t* samples, size_t count);

  // Stops recording and uploads the buffered clip. Returns true on a 2xx
  // response from Edge Impulse. No-op / returns false if nothing was
  // recorded.
  bool stopAndUpload();

  bool isRecording() const { return _recording; }
  size_t recordedSamples() const { return _writeIndex; }
  size_t maxSamples() const { return _maxSamples; }

 private:
  uint32_t _sampleRate;
  size_t _maxSamples;
  int16_t* _buffer;
  size_t _writeIndex = 0;
  bool _recording = false;
  const char* _label = "";
};
