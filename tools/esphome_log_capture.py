"""Subscribe to ESPHome native API logs and print them verbatim."""

from __future__ import annotations

import argparse
import asyncio
import os
import sys

from aioesphomeapi import APIClient
from aioesphomeapi.api_pb2 import LogLevel


async def capture(host: str, port: int, seconds: float, dump_config: bool) -> None:
    key = os.environ.get("ESPHOME_API_KEY")
    if not key:
        raise RuntimeError("ESPHOME_API_KEY is not set")

    client = APIClient(
        host,
        port,
        noise_psk=key,
        client_info="codex-sharp-ir-capture",
        timezone="UTC",
    )
    await client.connect(login=True)
    try:
        info = await client.device_info()
        print(
            f"Connected: name={info.name} mac={info.mac_address} "
            f"esphome={info.esphome_version} model={info.model}",
            flush=True,
        )

        def on_log(message: object) -> None:
            raw = getattr(message, "message", b"")
            text = raw.decode("utf-8", errors="replace") if isinstance(raw, bytes) else str(raw)
            print(text, flush=True)

        unsubscribe = client.subscribe_logs(
            on_log,
            log_level=LogLevel.LOG_LEVEL_VERY_VERBOSE,
            dump_config=dump_config,
        )
        try:
            if seconds <= 0:
                await asyncio.Event().wait()
            else:
                await asyncio.sleep(seconds)
        finally:
            unsubscribe()
    finally:
        await client.disconnect()


def main() -> None:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
        sys.stderr.reconfigure(encoding="utf-8", errors="replace")
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="sharp-ay-zp40kr.local")
    parser.add_argument("--port", type=int, default=6053)
    parser.add_argument("--seconds", type=float, default=10.0)
    parser.add_argument("--dump-config", action="store_true")
    args = parser.parse_args()
    try:
        asyncio.run(capture(args.host, args.port, args.seconds, args.dump_config))
    except KeyboardInterrupt:
        pass
    except Exception as exc:
        print(f"Connection failed: {type(exc).__name__}: {exc}", file=sys.stderr)
        raise SystemExit(1) from exc


if __name__ == "__main__":
    main()
