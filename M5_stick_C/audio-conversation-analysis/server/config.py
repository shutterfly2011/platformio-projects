from __future__ import annotations

import logging
from dataclasses import dataclass
from pathlib import Path

import yaml


@dataclass(frozen=True)
class TranscriptionConfig:
    enabled: bool
    model: str
    device: str
    compute_type: str
    language: str
    chunk_seconds: float
    min_chunk_seconds: float
    silence_ms: int

    @staticmethod
    def from_dict(raw: dict) -> "TranscriptionConfig":
        return TranscriptionConfig(
            enabled=bool(raw.get("enabled", False)),
            model=str(raw.get("model", "large-v3")),
            device=str(raw.get("device", "auto")),
            compute_type=str(raw.get("compute_type", "default")),
            language=str(raw.get("language", "auto")),
            chunk_seconds=float(raw.get("chunk_seconds", 10)),
            min_chunk_seconds=float(raw.get("min_chunk_seconds", 3)),
            silence_ms=int(raw.get("silence_ms", 500)),
        )


@dataclass(frozen=True)
class ServerConfig:
    listen_host: str
    listen_port: int
    storage_dir: Path
    mp3_bitrate: str
    ffmpeg_path: str
    timestamp_format: str
    log_level: str
    transcription: TranscriptionConfig

    @staticmethod
    def load(path: str | Path) -> "ServerConfig":
        path = Path(path)
        with path.open("r", encoding="utf-8") as f:
            raw = yaml.safe_load(f) or {}

        try:
            listen_port = int(raw["listen_port"])
        except (KeyError, TypeError, ValueError) as exc:
            raise ValueError("config: 'listen_port' must be an integer") from exc

        try:
            storage_dir = Path(raw["storage_dir"]).expanduser()
        except KeyError as exc:
            raise ValueError("config: 'storage_dir' is required") from exc

        config = ServerConfig(
            listen_host=str(raw.get("listen_host", "0.0.0.0")),
            listen_port=listen_port,
            storage_dir=storage_dir,
            mp3_bitrate=str(raw.get("mp3_bitrate", "96k")),
            ffmpeg_path=str(raw.get("ffmpeg_path", "ffmpeg")),
            timestamp_format=str(raw.get("timestamp_format", "%Y%m%d_%H%M%S")),
            log_level=str(raw.get("log_level", "INFO")).upper(),
            transcription=TranscriptionConfig.from_dict(raw.get("transcription") or {}),
        )

        config.storage_dir.mkdir(parents=True, exist_ok=True)
        return config

    def configure_logging(self) -> None:
        level = getattr(logging, self.log_level, logging.INFO)
        logging.basicConfig(
            level=level,
            format="%(asctime)s %(levelname)-8s %(name)s: %(message)s",
        )
