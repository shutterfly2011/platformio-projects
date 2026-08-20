#!/usr/bin/env python3
"""Simulate an M5StickC Plus2 device streaming audio to the server.

Speaks the same protocol the firmware uses: connect, send a JSON header
line, then stream raw PCM16LE audio, then disconnect. Useful for testing
the server pipeline end-to-end without hardware.

Examples:
    # Stream 5 seconds of a generated tone
    python3 simulate_device.py --host 127.0.0.1 --port 5050 --seconds 5

    # Stream an existing WAV file (must be 16-bit PCM)
    python3 simulate_device.py --host 127.0.0.1 --port 5050 --wav sample.wav
"""
from __future__ import annotations

import argparse
import array
import json
import math
import socket
import time
import wave


def generate_tone(seconds: float, sample_rate: int, freq: float = 440.0) -> bytes:
    n_samples = int(seconds * sample_rate)
    samples = array.array("h")
    amplitude = 12000
    for i in range(n_samples):
        value = int(amplitude * math.sin(2 * math.pi * freq * (i / sample_rate)))
        samples.append(value)
    return samples.tobytes()


def load_wav(path: str) -> tuple[bytes, int, int]:
    with wave.open(path, "rb") as wf:
        if wf.getsampwidth() != 2:
            raise ValueError("WAV file must be 16-bit PCM")
        channels = wf.getnchannels()
        sample_rate = wf.getframerate()
        pcm = wf.readframes(wf.getnframes())
    return pcm, sample_rate, channels


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=5050)
    parser.add_argument("--wav", help="path to a 16-bit PCM WAV file to stream")
    parser.add_argument("--seconds", type=float, default=5.0, help="tone duration if --wav not given")
    parser.add_argument("--sample-rate", type=int, default=16000)
    parser.add_argument("--channels", type=int, default=1)
    parser.add_argument("--device-id", default="simulator")
    parser.add_argument("--chunk-bytes", type=int, default=1024)
    parser.add_argument("--realtime", action="store_true", help="pace sending to match real playback speed")
    args = parser.parse_args()

    if args.wav:
        pcm, sample_rate, channels = load_wav(args.wav)
    else:
        sample_rate, channels = args.sample_rate, args.channels
        pcm = generate_tone(args.seconds, sample_rate)

    header = {
        "device_id": args.device_id,
        "sample_rate": sample_rate,
        "bits": 16,
        "channels": channels,
        "start_ts_ms": int(time.time() * 1000),
    }

    print(f"connecting to {args.host}:{args.port}")
    with socket.create_connection((args.host, args.port)) as sock:
        sock.sendall((json.dumps(header) + "\n").encode("utf-8"))
        print(f"sent header: {header}")

        bytes_per_sample = 2 * channels
        seconds_per_chunk = args.chunk_bytes / bytes_per_sample / sample_rate

        total_sent = 0
        for offset in range(0, len(pcm), args.chunk_bytes):
            chunk = pcm[offset : offset + args.chunk_bytes]
            sock.sendall(chunk)
            total_sent += len(chunk)
            if args.realtime:
                time.sleep(seconds_per_chunk)

        print(f"sent {total_sent} bytes of PCM, closing connection")

    print("done")


if __name__ == "__main__":
    main()
