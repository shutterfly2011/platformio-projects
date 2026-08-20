from __future__ import annotations

import argparse
import asyncio
import functools
import logging
import signal

from config import ServerConfig
from recorder import handle_connection

logger = logging.getLogger("main")


async def run(config: ServerConfig) -> None:
    transcription = None
    if config.transcription.enabled:
        from transcriber import TranscriptionService

        transcription = TranscriptionService(config.transcription)

    handler = functools.partial(handle_connection, config=config, transcription=transcription)
    server = await asyncio.start_server(handler, config.listen_host, config.listen_port)

    addrs = ", ".join(str(sock.getsockname()) for sock in server.sockets)
    logger.info("listening on %s", addrs)
    logger.info("storing recordings in %s", config.storage_dir)
    logger.info("transcription: %s", "enabled" if transcription else "disabled")

    loop = asyncio.get_running_loop()
    stop_event = asyncio.Event()
    for sig in (signal.SIGINT, signal.SIGTERM):
        loop.add_signal_handler(sig, stop_event.set)

    try:
        async with server:
            await stop_event.wait()
            logger.info("shutting down")
    finally:
        if transcription is not None:
            transcription.close()


def main() -> None:
    parser = argparse.ArgumentParser(description="Audio streaming recorder server")
    parser.add_argument(
        "--config", default="config.yaml", help="path to config.yaml (default: %(default)s)"
    )
    args = parser.parse_args()

    config = ServerConfig.load(args.config)
    config.configure_logging()

    asyncio.run(run(config))


if __name__ == "__main__":
    main()
