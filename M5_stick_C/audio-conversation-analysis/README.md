# Audio Conversation Analysis — Streaming Recorder

## Purpose

A portable, button-operated recorder for capturing in-person conversations:
an M5StickC Plus2 worn or carried on you streams microphone audio over WiFi
to a server on your local network the moment you press its button, so there's
no on-device storage to manage and no separate transfer step afterward. The
server archives each conversation as a timestamped MP3 and, optionally,
transcribes it live into a subtitle file — turning a raw recording into
searchable text for later review or analysis.

Two parts:

- **`firmware/`** — M5StickC Plus2 firmware (PlatformIO). Press the front
  button (BtnA) to start/stop streaming microphone audio to a server over
  WiFi. The screen shows WiFi/battery/recording status plus a help line.
- **`server/`** — Python server for a Linux box. Listens for incoming audio
  streams, encodes them to MP3 with `ffmpeg`, and saves each recording as
  `<start-timestamp>-<end-timestamp>.mp3` in a configurable directory.
  Optionally transcribes each recording live with `faster-whisper` and saves
  a matching `<start-timestamp>-<end-timestamp>.srt` alongside it.

## Wire protocol

The device speaks a small custom protocol over a plain TCP socket (no HTTP):

1. Device opens a TCP connection to the server.
2. Device sends one line of UTF-8 JSON terminated by `\n`:
   ```json
   {"device_id": "m5stick-01", "sample_rate": 16000, "bits": 16, "channels": 1}
   ```
3. Device streams raw little-endian PCM16 audio bytes continuously.
4. Device closes the socket to end the recording.

Only 16-bit PCM is supported. The server timestamps the recording using its
own clock (connection-accept time as start, disconnect time as end) rather
than trusting the device's clock, so the firmware doesn't need NTP sync.

If you change this protocol, update both `firmware/src/main.cpp` and
`server/recorder.py` together.

## Firmware setup

Requires [PlatformIO](https://platformio.org/) (CLI or VS Code extension).

```bash
cd firmware
cp include/config.h.example include/config.h
# edit config.h: WiFi credentials, server host/port
pio run -t upload
pio device monitor   # optional, for serial logs
```

`config.h` is gitignored so WiFi credentials never get committed.

## Server setup

Requires Python 3.9+ and `ffmpeg` on PATH (`apt install ffmpeg` on
Debian/Ubuntu).

```bash
cd server
python3 -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt
cp config.yaml.example config.yaml
# edit config.yaml: storage_dir, listen_port, mp3_bitrate, etc.
python3 main.py --config config.yaml
```

### Testing without hardware

`scripts/simulate_device.py` speaks the same protocol as the firmware, so
you can validate the whole pipeline without a physical device:

```bash
python3 scripts/simulate_device.py --host 127.0.0.1 --port 5050 --seconds 5
```

Check `storage_dir` for the resulting MP3 file.

### Live transcription (optional)

Set `transcription.enabled: true` in `config.yaml` to have the server
transcribe each recording as it streams in, using
[faster-whisper](https://github.com/SYSTRAN/faster-whisper), and save an
`.srt` subtitle file alongside the MP3 (matching filename, same
start/end-timestamp naming).

This is entirely optional and off by default — leave it disabled on servers
without the GPU/CPU headroom to run a Whisper model. The core recording
pipeline has zero dependency on it either way: `faster-whisper` and
`webrtcvad` are only imported when the feature is turned on, so a box without
them installed still runs the base recorder fine.

To enable it:

```bash
pip install -r requirements-transcription.txt
```

Then in `config.yaml`:

```yaml
transcription:
  enabled: true
  model: "large-v3"       # any faster-whisper model name, or a local/HF path
  device: "auto"          # "cpu" | "cuda" | "auto"
  compute_type: "default" # e.g. "float16", "int8_float16", "int8"
  language: "auto"        # or a pinned code like "zh", "en"
  chunk_seconds: 10
  min_chunk_seconds: 3
  silence_ms: 500
```

Notes:
- GPU use requires CUDA 12 + cuDNN 9 (see faster-whisper's docs); CPU mode
  (`device: cpu`) also works, just slower.
- Only 16kHz audio is transcribed — the firmware's default `SAMPLE_RATE`
  already matches this. Streams at a different sample rate are recorded
  normally but skipped for transcription (logged as a warning).
- Audio is split into chunks at detected pauses (via voice-activity
  detection) rather than a hard clock boundary, so cuts don't land mid-word.
  A `chunk_seconds` ceiling still forces a cut during continuous
  uninterrupted speech, and the previous chunk's text is fed back in as
  context for the next one, so transcription stays coherent across cuts.
- Transcripts are always kept in the source language (never translated).
  With `language: "auto"`, the language is detected from the first chunk of
  a recording and then locked in for the rest of it.

### Running as a systemd service

An example unit is in `deploy/audio-stream-server.service`. Adjust the
`User`, `WorkingDirectory`, and `ExecStart` paths for your deployment, then:

```bash
sudo cp deploy/audio-stream-server.service /etc/systemd/system/
sudo mkdir -p /etc/audio-stream-server
sudo cp server/config.yaml /etc/audio-stream-server/config.yaml
sudo systemctl daemon-reload
sudo systemctl enable --now audio-stream-server
```

## Notes / future extensions

- No authentication on the stream socket — intended for trusted local
  networks only. Add a shared-secret check in the JSON header and
  `recorder.py` if the server is reachable beyond that.
- Only one recording per TCP connection; the device makes a new connection
  each time it starts recording.
