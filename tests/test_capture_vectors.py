from __future__ import annotations

import sys
from pathlib import Path
import unittest

ROOT = Path(__file__).parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from analyze_packets import (  # noqa: E402
    HEADER,
    classify,
    load_captures,
    parse_packet,
    valid_checksum,
)


CAPTURE_PATH = ROOT / "tests" / "captures" / "a788_session_2026-08-04.json"
CAPTURES = load_captures(CAPTURE_PATH)
BY_ID = {capture["id"]: parse_packet(capture["bytes_hex"]) for capture in CAPTURES}


class CaptureVectorTests(unittest.TestCase):
    def test_all_55_real_captures_are_present_and_unique(self):
        self.assertEqual(55, len(CAPTURES))
        self.assertEqual(len(CAPTURES), len(BY_ID))

    def test_every_capture_has_header_and_valid_checksum(self):
        for capture_id, packet in BY_ID.items():
            with self.subTest(capture=capture_id):
                self.assertEqual(HEADER, packet[:4])
                self.assertTrue(valid_checksum(packet))
                self.assertEqual(0x01, packet[12] & 0x0F)

    def test_reserved_bytes_and_nibbles_match_observed_baseline(self):
        for capture_id, packet in BY_ID.items():
            with self.subTest(capture=capture_id):
                self.assertEqual(0, packet[4] & 0xF0)
                self.assertEqual(1, packet[5] & 0x0F)
                self.assertEqual(0x80, packet[9])
                self.assertIn(packet[11], (0xF0, 0xF4))

    def test_heat_temperature_mapping_is_linear_18_through_32(self):
        for temperature in range(18, 33):
            packet = BY_ID[f"heat_temp_{temperature}"]
            self.assertEqual(temperature - 17, packet[4] & 0x0F)
            self.assertEqual(1, packet[6] & 0x03)
            self.assertEqual(0x04, packet[10])

    def test_vacancy_is_heat_code_zero_and_exits_to_18(self):
        on = BY_ID["vacancy_on"]
        off = BY_ID["vacancy_off"]
        self.assertEqual((1, 0, "vacancy"), (on[6] & 0x03, on[4], classify(on)))
        self.assertEqual((1, 1, "temperature"), (off[6] & 0x03, off[4], classify(off)))

    def test_all_fan_codes(self):
        expected = {
            "fan_auto": 2,
            "fan_level_1": 3,
            "fan_level_2": 5,
            "fan_level_3": 7,
        }
        for capture_id, code in expected.items():
            packet = BY_ID[capture_id]
            self.assertEqual(code, (packet[6] >> 4) & 0x07)
            self.assertEqual("fan", classify(packet))

    def test_all_spot_air_wire_values(self):
        expected = {
            "spot_off": 0x08,
            "spot_near_left": 0x2C,
            "spot_near_center": 0x1C,
            "spot_near_right": 0x3C,
            "spot_far_left": 0x2B,
            "spot_far_center": 0x1B,
            "spot_far_right": 0x3B,
        }
        for capture_id, wire_value in expected.items():
            packet = BY_ID[capture_id]
            self.assertEqual(wire_value, packet[8])
            self.assertEqual("spot_air", classify(packet))

    def test_stateless_swing_packets(self):
        vertical = BY_ID["vertical_swing_toggle"]
        horizontal = BY_ID["horizontal_swing_toggle"]
        self.assertEqual((0x0F, 0x06, "vertical_swing"), (vertical[8], vertical[10], classify(vertical)))
        self.assertEqual((0xF8, 0x07, "horizontal_swing"), (horizontal[8], horizontal[10], classify(horizontal)))

    def test_special_action_classes(self):
        for name, payload in (("powerful", 0x01), ("eco", 0x20)):
            on = BY_ID[f"{name}_on"]
            off = BY_ID[f"{name}_off"]
            self.assertEqual((6, payload, name), (on[5] >> 4, on[10], classify(on)))
            self.assertEqual((7, payload, name), (off[5] >> 4, off[10], classify(off)))

    def test_ordinary_start_stop_clock_fields(self):
        stop = BY_ID["heat_18_stop_clock_18_26"]
        start = BY_ID["heat_18_start_clock_18_27"]
        self.assertEqual((18, 26), (stop[7], stop[10]))
        self.assertEqual((18, 27), (start[7], start[10]))

    def test_powered_mode_changes_use_update_action_and_zero_payload(self):
        for capture_id in ("heat_18_start_after_powerful", "cool_21_start", "dry_start"):
            packet = BY_ID[capture_id]
            self.assertEqual((3, 0, "mode"), (packet[5] >> 4, packet[10], classify(packet)))


if __name__ == "__main__":
    unittest.main()
