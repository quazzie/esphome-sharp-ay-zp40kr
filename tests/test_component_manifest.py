from __future__ import annotations

import ast
from pathlib import Path
import unittest


ROOT = Path(__file__).parents[1]
CLIMATE_PLATFORM = ROOT / "components" / "sharp_ay_zp40kr" / "climate.py"


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


if __name__ == "__main__":
    unittest.main()
