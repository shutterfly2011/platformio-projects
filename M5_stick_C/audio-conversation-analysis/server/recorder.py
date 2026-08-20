from __future__ import annotations

import asyncio
import json
import logging
import uuid
from dataclasses import dataclass
from datetime import datetime

from config import ServerConfig
from transcriber import SrtWriter, TranscriptionService

logger = logging.getLogger("recorder")

READ_CHUNK_SIZE = 4096
HEADER_LINE_LIMIT = 4096

TRANSCRIPTION_SAMPLE_RATE = 16000  # required by both faster-whisper and webrtcvad


@dataclass
class StreamHeader:
    device_id: str
    sample_rate: int
    bits: int
    channels: int
    start_ts_ms: int | None


class ProtocolError(ValueError):
    pass


def parse_header(line: bytes) -> StreamHeader:
    try:
        obj = json.loads(line.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise ProtocolError(f"malformed header JSON: {exc}") from exc

    try:
        sample_rate = int(obj["sample_rate"])
        bits = int(obj["bits"])
        channels = int(obj["channels"])
    except (KeyError, TypeError, ValueError) as exc:
        raise ProtocolError(f"missing/invalid audio format fields: {exc}") from exc

    if bits != 16:
        raise ProtocolError(f"unsupported bit depth {bits}, only 16-bit PCM is supported")

    return StreamHeader(
        device_id=str(obj.get("device_id", "unknown")),
        sample_rate=sample_rate,
        bits=bits,
        channels=channels,
        start_ts_ms=obj.get("start_ts_ms"),
    )


async def _read_header_line(reader: asyncio.StreamReader) -> bytes:
    try:
        line = await asyncio.wait_for(reader.readline(), timeout=10)
    except asyncio.TimeoutError as exc:
        raise ProtocolError("timed out waiting for header line") from exc
    if not line:
        raise ProtocolError("connection closed before header was received")
    if len(line) > HEADER_LINE_LIMIT:
        raise ProtocolError("header line too long")
    return line


async def _transcribe_worker(
    queue: asyncio.Queue,
    service: TranscriptionService,
    srt_writer: SrtWriter,
    sample_rate: int,
    configured_language: str,
    peer,
) -> None:
    offset_s = 0.0
    prompt: str | None = None
    effective_language = None if configured_language == "auto" else configured_language

    while True:
        pcm_bytes = await queue.get()
        if pcm_bytes is None:
            break
        try:
            segments, detected_language = await service.transcribe_chunk(
                pcm_bytes, sample_rate, effective_language, prompt,
            )
        except Exception:
            logger.exception("transcription failed for a chunk from %s", peer)
            offset_s += len(pcm_bytes) / 2 / sample_rate
            continue

        if effective_language is None:
            effective_language = detected_language
            logger.info("detected transcription language '%s' for %s", detected_language, peer)

        for start_s, end_s, text in segments:
            srt_writer.write_segment(offset_s + start_s, offset_s + end_s, text)
            prompt = text.strip() or prompt

        offset_s += len(pcm_bytes) / 2 / sample_rate


async def handle_connection(
    reader: asyncio.StreamReader,
    writer: asyncio.StreamWriter,
    config: ServerConfig,
    transcription: TranscriptionService | None = None,
) -> None:
    peer = writer.get_extra_info("peername")
    logger.info("connection from %s", peer)

    try:
        header_line = await _read_header_line(reader)
        header = parse_header(header_line)
    except ProtocolError as exc:
        logger.warning("rejecting connection from %s: %s", peer, exc)
        writer.close()
        await writer.wait_closed()
        return

    logger.info(
        "stream started: device=%s rate=%d bits=%d channels=%d",
        header.device_id, header.sample_rate, header.bits, header.channels,
    )

    start_dt = (
        datetime.fromtimestamp(header.start_ts_ms / 1000)
        if header.start_ts_ms
        else datetime.now()
    )

    stream_id = uuid.uuid4().hex
    tmp_path = config.storage_dir / f".{stream_id}.tmp.mp3"

    transcribe_enabled = transcription is not None
    if transcribe_enabled and header.sample_rate != TRANSCRIPTION_SAMPLE_RATE:
        logger.warning(
            "skipping transcription for %s: sample_rate %d != %d",
            peer, header.sample_rate, TRANSCRIPTION_SAMPLE_RATE,
        )
        transcribe_enabled = False

    chunker = None
    srt_writer = None
    chunk_queue: asyncio.Queue | None = None
    transcribe_task: asyncio.Task | None = None
    srt_tmp_path = config.storage_dir / f".{stream_id}.tmp.srt"

    if transcribe_enabled:
        chunker = transcription.new_chunker(header.sample_rate, config.transcription)
        srt_writer = SrtWriter(srt_tmp_path)
        chunk_queue = asyncio.Queue()
        transcribe_task = asyncio.create_task(
            _transcribe_worker(
                chunk_queue, transcription, srt_writer,
                header.sample_rate, config.transcription.language, peer,
            )
        )

    ffmpeg_cmd = [
        config.ffmpeg_path,
        "-hide_banner",
        "-loglevel", "error",
        "-f", "s16le",
        "-ar", str(header.sample_rate),
        "-ac", str(header.channels),
        "-i", "pipe:0",
        "-codec:a", "libmp3lame",
        "-b:a", config.mp3_bitrate,
        "-y",
        str(tmp_path),
    ]

    try:
        process = await asyncio.create_subprocess_exec(
            *ffmpeg_cmd,
            stdin=asyncio.subprocess.PIPE,
            stdout=asyncio.subprocess.DEVNULL,
            stderr=asyncio.subprocess.PIPE,
        )
    except FileNotFoundError:
        logger.error(
            "ffmpeg not found at '%s' - install ffmpeg or fix ffmpeg_path in config",
            config.ffmpeg_path,
        )
        writer.close()
        await writer.wait_closed()
        return

    bytes_received = 0
    try:
        while True:
            chunk = await reader.read(READ_CHUNK_SIZE)
            if not chunk:
                break
            bytes_received += len(chunk)
            try:
                process.stdin.write(chunk)
                await process.stdin.drain()
            except (BrokenPipeError, ConnectionResetError):
                logger.error("ffmpeg process pipe closed unexpectedly")
                break
            if transcribe_enabled:
                for piece in chunker.feed(chunk):
                    chunk_queue.put_nowait(piece)
    finally:
        if process.stdin and not process.stdin.is_closing():
            process.stdin.close()
        stderr_output = b""
        try:
            _, stderr_output = await asyncio.wait_for(process.communicate(), timeout=15)
        except asyncio.TimeoutError:
            logger.warning("ffmpeg did not exit in time, killing it")
            process.kill()
            await process.wait()

        if transcribe_enabled:
            remainder = chunker.flush()
            if remainder:
                chunk_queue.put_nowait(remainder)
            chunk_queue.put_nowait(None)
            try:
                await asyncio.wait_for(transcribe_task, timeout=60)
            except asyncio.TimeoutError:
                logger.warning("transcription did not finish in time for %s", peer)
                transcribe_task.cancel()
            srt_writer.close()

        writer.close()
        await writer.wait_closed()

    end_dt = datetime.now()

    if process.returncode != 0:
        logger.error(
            "ffmpeg exited with code %s: %s",
            process.returncode, stderr_output.decode(errors="replace").strip(),
        )
        tmp_path.unlink(missing_ok=True)
        if transcribe_enabled:
            srt_tmp_path.unlink(missing_ok=True)
        return

    if bytes_received == 0:
        logger.info("no audio received from %s, discarding empty recording", peer)
        tmp_path.unlink(missing_ok=True)
        if transcribe_enabled:
            srt_tmp_path.unlink(missing_ok=True)
        return

    base_name = (
        f"{start_dt.strftime(config.timestamp_format)}"
        f"-{end_dt.strftime(config.timestamp_format)}"
    )
    final_path = config.storage_dir / f"{base_name}.mp3"
    suffix = 1
    while final_path.exists():
        final_path = config.storage_dir / f"{base_name}_{suffix}.mp3"
        suffix += 1
    tmp_path.rename(final_path)
    final_stem = final_path.stem

    if transcribe_enabled:
        if srt_writer.has_content:
            srt_tmp_path.rename(config.storage_dir / f"{final_stem}.srt")
        else:
            srt_tmp_path.unlink(missing_ok=True)

    duration_s = (end_dt - start_dt).total_seconds()
    logger.info(
        "saved recording %s (%.1fs, %d bytes PCM) from %s",
        final_path, duration_s, bytes_received, peer,
    )
