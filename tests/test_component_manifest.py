from __future__ import annotations

import ast
from pathlib import Path
import unittest


ROOT = Path(__file__).parents[1]
CLIMATE_PLATFORM = ROOT / "components" / "sharp_ay_zp40kr" / "climate.py"
CLIMATE_HEADER = ROOT / "components" / "sharp_ay_zp40kr" / "sharp_ay_zp40kr.h"
CLIMATE_SOURCE = ROOT / "components" / "sharp_ay_zp40kr" / "sharp_ay_zp40kr.cpp"


class ComponentManifestTests(unittest.TestCase):
    def test_cpp_header_dependencies_are_auto_loaded(self):
        tree = ast.parse(CLIMATE_PLATFORM.read_text(encoding="utf-8"))
        auto_load = None
        for node in tree.body:
            if isinstance(node, ast.Assign) and any(
                isinstance(target, ast.Name) and target.id == "AUTO_LOAD"
                for target in node.targets
            ):
                auto_load = ast.literal_eval(node.value)
                break

        self.assertIsNotNone(auto_load)
        self.assertIn("time", auto_load)

    def test_component_advertises_only_supported_climate_modes(self):
        header = CLIMATE_HEADER.read_text(encoding="utf-8")
        source = CLIMATE_SOURCE.read_text(encoding="utf-8")
        traits_body = source.split("SharpAyZp40krClimate::traits()", 1)[1].split(
            "void SharpAyZp40krClimate::sync_shadow_from_climate_", 1
        )[0]

        self.assertIn("ClimateTraits traits() override", header)
        self.assertIn("climate_ir::ClimateIR::traits()", traits_body)
        for mode in ("OFF", "COOL", "HEAT", "DRY"):
            self.assertIn(f"CLIMATE_MODE_{mode}", traits_body)
        self.assertNotIn("CLIMATE_MODE_HEAT_COOL", traits_body)
        self.assertNotIn("CLIMATE_MODE_AUTO", traits_body)


if __name__ == "__main__":
    unittest.main()
