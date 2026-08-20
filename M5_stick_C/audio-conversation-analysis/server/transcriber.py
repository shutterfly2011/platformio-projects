from __future__ import annotations

import asyncio
import logging
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

from config import TranscriptionConfig

logger = logging.getLogger("transcriber")

FRAME_MS = 30
BYTES_PER_SAMPLE = 2  # 16-bit PCM
VAD_MODE = 2  # 0 (least aggressive) - 3 (most aggressive) about classifying audio as silence


class AudioChunker:
    """Splits a live PCM16 mono stream into chunks at detected pauses.

    Cutting at a voice-activity pause (rather than a hard clock boundary)
    avoids slicing through the middle of a word/sentence most of the time.
    A max length still forces a cut during continuous uninterrupted speech
    so latency stays bounded.
    """

    def __init__(
        self,
        sample_rate: int,
        chunk_seconds: float,
        min_chunk_seconds: float,
        silence_ms: int,
    ) -> None:
        import webrtcvad

        self.sample_rate = sample_rate
        self._frame_bytes = int(sample_rate * FRAME_MS / 1000) * BYTES_PER_SAMPLE
        self._max_chunk_bytes = int(sample_rate * chunk_seconds) * BYTES_PER_SAMPLE
        self._min_chunk_bytes = int(sample_rate * min_chunk_seconds) * BYTES_PER_SAMPLE
        self._silence_frames_needed = max(1, silence_ms // FRAME_MS)
        self._vad = webrtcvad.Vad(VAD_MODE)

        self._pending = bytearray()
        self._chunk = bytearray()
        self._trailing_silence_frames = 0

    def feed(self, data: bytes) -> list[bytes]:
        self._pending.extend(data)
        completed: list[bytes] = []

        while len(self._pending) >= self._frame_bytes:
            frame = bytes(self._pending[: self._frame_bytes])
            del self._pending[: self._frame_bytes]
            self._chunk.extend(frame)

            if self._vad.is_speech(frame, self.sample_rate):
                self._trailing_silence_frames = 0
            else:
                self._trailing_silence_frames += 1

            chunk_len = len(self._chunk)
            hit_max = chunk_len >= self._max_chunk_bytes
            hit_pause = (
                chunk_len >= self._min_chunk_bytes
                and self._trailing_silence_frames >= self._silence_frames_needed
            )
            if hit_max or hit_pause:
                completed.append(self._cut())

        return completed

    def flush(self) -> bytes | None:
        remainder = bytes(self._chunk) + bytes(self._pending)
        self._chunk = bytearray()
        self._pending = bytearray()
        self._trailing_silence_frames = 0
        return remainder if remainder else None

    def _cut(self) -> bytes:
        data = bytes(self._chunk)
        self._chunk = bytearray()
        self._trailing_silence_frames = 0
        return data


class TranscriptionService:
    """Loads a faster-whisper model once and runs inference off the event loop."""

    def __init__(self, config: TranscriptionConfig) -> None:
        from faster_whisper import WhisperModel

        logger.info(
            "loading faster-whisper model '%s' (device=%s, compute_type=%s) ...",
            config.model, config.device, config.compute_type,
        )
        self._model = WhisperModel(
            config.model, device=config.device, compute_type=config.compute_type
        )
        self._executor = ThreadPoolExecutor(max_workers=1, thread_name_prefix="whisper")
        logger.info("transcription model ready")

    def new_chunker(self, sample_rate: int, config: TranscriptionConfig) -> AudioChunker:
        return AudioChunker(
            sample_rate=sample_rate,
            chunk_seconds=config.chunk_seconds,
            min_chunk_seconds=config.min_chunk_seconds,
            silence_ms=config.silence_ms,
        )

    async def transcribe_chunk(
        self,
        pcm_bytes: bytes,
        sample_rate: int,
        language: str | None,
        initial_prompt: str | None,
    ) -> tuple[list[tuple[float, float, str]], str]:
        loop = asyncio.get_running_loop()
        return await loop.run_in_executor(
            self._executor,
            self._transcribe_blocking,
            pcm_bytes, sample_rate, language, initial_prompt,
        )

    def _transcribe_blocking(
        self,
        pcm_bytes: bytes,
        sample_rate: int,
        language: str | None,
        initial_prompt: str | None,
    ) -> tuple[list[tuple[float, float, str]], str]:
        import numpy as np

        audio = np.frombuffer(pcm_bytes, dtype=np.int16).astype(np.float32) / 32768.0
        segments, info = self._model.transcribe(
            audio,
            language=None if language in (None, "auto") else language,
            task="transcribe",
            initial_prompt=initial_prompt,
            condition_on_previous_text=True,
        )
        results = [(seg.start, seg.end, seg.text) for seg in segments]
        return results, info.language

    def close(self) -> None:
        self._executor.shutdown(wait=False)


def format_srt_timestamp(seconds: float) -> str:
    total_ms = max(0, int(round(seconds * 1000)))
    hours, total_ms = divmod(total_ms, 3_600_000)
    minutes, total_ms = divmod(total_ms, 60_000)
    secs, ms = divmod(total_ms, 1000)
    return f"{hours:02d}:{minutes:02d}:{secs:02d},{ms:03d}"


class SrtWriter:
    def __init__(self, path: Path) -> None:
        self.path = path
        self._file = path.open("w", encoding="utf-8")
        self._index = 1

    def write_segment(self, start_s: float, end_s: float, text: str) -> None:
        text = text.strip()
        if not text:
            return
        self._file.write(f"{self._index}\n")
        self._file.write(f"{format_srt_timestamp(start_s)} --> {format_srt_timestamp(end_s)}\n")
        self._file.write(f"{text}\n\n")
        self._file.flush()
        self._index += 1

    @property
    def has_content(self) -> bool:
        return self._index > 1

    def close(self) -> None:
        self._file.close()
