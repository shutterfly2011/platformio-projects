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
  // startDelayMs: mic frames fed within this long after startRecording()
  // are discarded rather than written to the buffer, so the physical click
  // of the button that triggered the recording doesn't end up in the clip.
  EdgeImpulseUploader(uint32_t sampleRate, size_t maxSamples,
                       uint32_t startDelayMs = 0);
  ~EdgeImpulseUploader();

  // Allocates the recording buffer. Must be called once from setup() before
  // any other method — NOT from the constructor, because this object is
  // instantiated at global scope and C++ global constructors run during
  // ESP32 static-init, before the heap allocator is fully set up; a heap
  // allocation there throws bad_alloc, and since Arduino-ESP32 builds
  // without C++ exceptions by default, that aborts the boot in a crash
  // loop. Returns false if the allocation failed.
  bool begin();

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
  uint32_t _startDelayMs;
  int16_t* _buffer = nullptr;
  size_t _writeIndex = 0;
  bool _recording = false;
  uint32_t _recordingStartMs = 0;
  const char* _label = "";
};
