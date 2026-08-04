#!/usr/bin/env python3
"""Validate and summarize CRMC-A788JBEZ capture packets.

This tool intentionally has no ESPHome dependency. It is useful while capturing
new packets on a workstation and is also the reference used by the test suite.
"""

from __future__ import annotations

import argparse
from collections import Counter
import json
from pathlib import Path
from typing import Any

PACKET_SIZE = 13
HEADER = bytes.fromhex("AA 5A CF 10")


def parse_packet(value: str | list[str]) -> bytes:
    if isinstance(value, list):
        value = " ".join(value)
    packet = bytes.fromhex(value)
    if len(packet) != PACKET_SIZE:
        raise ValueError(f"expected {PACKET_SIZE} bytes, got {len(packet)}")
    return packet


def calculate_checksum(packet: bytes) -> int:
    if len(packet) != PACKET_SIZE:
        raise ValueError(f"expected {PACKET_SIZE} bytes, got {len(packet)}")
    value = 0
    for byte in packet[:12]:
        value ^= byte
    value ^= packet[12] & 0x0F
    value ^= value >> 4
    return value & 0x0F


def valid_checksum(packet: bytes) -> bool:
    return packet[12] >> 4 == calculate_checksum(packet)


def classify(packet: bytes) -> str:
    action = packet[5] >> 4
    payload = packet[10]
    airflow = packet[8]
    if (packet[6] & 0x0F) == 4 and action == 1 and payload == 0x33:
        return "ion_only_start"
    if action == 2 and payload == 0x33:
        return "ion_only_stop"
    if action in (6, 7):
        return {0x00: "ion", 0x01: "powerful", 0x20: "eco"}.get(
            payload, "unknown_special"
        )
    if action == 3:
        if payload == 0x00:
            return "mode"
        if payload == 0x04:
            return "vacancy" if (packet[6] & 0x03) == 1 and packet[4] == 0 else "temperature"
        if payload == 0x05:
            return "fan"
        if payload == 0x06:
            if airflow == 0x0F:
                return "vertical_swing"
            if airflow in (0x0D, 0x0E):
                return "coanda"
        if payload == 0x07 and airflow == 0xF8:
            return "horizontal_swing"
        if payload == 0x09:
            return "spot_air"
    if action == 1:
        return "start_or_mode"
    if action == 2:
        return "stop"
    return "unknown"


def inspect_capture(capture: dict[str, Any]) -> dict[str, Any]:
    packet = parse_packet(capture["bytes_hex"])
    mode = packet[6] & 0x03
    temp_code = packet[4] & 0x0F
    return {
        "id": capture["id"],
        "packet": packet,
        "header_valid": packet[:4] == HEADER,
        "checksum_valid": valid_checksum(packet),
        "command": classify(packet),
        "action": packet[5] >> 4,
        "mode": {1: "heat", 2: "cool", 3: "dry"}.get(mode, f"unknown:{mode}"),
        "temperature": None if temp_code == 0 else temp_code + 17,
        "fan_code": (packet[6] >> 4) & 0x07,
        "airflow": packet[8],
        "ion_memory": bool(packet[11] & 0x04),
    }


def load_captures(path: Path) -> list[dict[str, Any]]:
    document = json.loads(path.read_text(encoding="utf-8"))
    captures = document.get("captures")
    if not isinstance(captures, list):
        raise ValueError("capture document has no 'captures' list")
    return captures


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "capture_file",
        nargs="?",
        type=Path,
        default=Path(__file__).parents[1]
        / "tests"
        / "captures"
        / "a788_session_2026-08-04.json",
    )
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args()

    inspected = [inspect_capture(item) for item in load_captures(args.capture_file)]
    failures = [
        item
        for item in inspected
        if not item["header_valid"] or not item["checksum_valid"]
    ]
    commands = Counter(item["command"] for item in inspected)

    print(f"captures: {len(inspected)}")
    print(f"valid: {len(inspected) - len(failures)}")
    print("commands:")
    for command, count in sorted(commands.items()):
        print(f"  {command}: {count}")
    if args.verbose:
        for item in inspected:
            print(
                f"{item['id']}: {item['command']} mode={item['mode']} "
                f"temp={item['temperature']} fan={item['fan_code']} "
                f"airflow={item['airflow']:02X} ion={item['ion_memory']}"
            )
    if failures:
        for item in failures:
            print(
                f"INVALID {item['id']}: header={item['header_valid']} "
                f"checksum={item['checksum_valid']}"
            )
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
