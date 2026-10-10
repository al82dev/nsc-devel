from __future__ import annotations

import sys
import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPOSITORY_ROOT))

import nscbuilder3 as builder


def is_compiler_component(component: str) -> bool:
    return (
        component.startswith("Host.")
        or component.startswith("SelfHost.")
        or component in {"BuildSystem", "Packaging"}
    )


class Builder3CatalogScopeTests(unittest.TestCase):
    def test_live_catalog_equals_compiler_implementation_waves(self) -> None:
        waves = builder.read_csv(builder.WAVES_PATH)
        wave_components = {row["component"] for row in waves}
        catalog = builder.CanonicalCatalog.load()

        self.assertTrue(wave_components)
        self.assertTrue(all(is_compiler_component(name) for name in wave_components))
        self.assertEqual(set(catalog.components), wave_components)
        self.assertEqual({case.component for case in catalog.cases}, wave_components)

    def test_external_inventory_components_never_enter_executable_manifest(self) -> None:
        wave_components = {
            row["component"] for row in builder.read_csv(builder.WAVES_PATH)
        }
        inventory_components = {
            row["component"] for row in builder.read_csv(builder.COMPONENTS_PATH)
        }
        external_components = inventory_components - wave_components
        manifest_components = {
            str(row["component"]) for row in builder.read_jsonl(builder.MANIFEST_PATH)
        }

        self.assertTrue(external_components)
        self.assertTrue(all(not is_compiler_component(name) for name in external_components))
        self.assertEqual(manifest_components - wave_components, set())
        self.assertEqual(manifest_components & external_components, set())


if __name__ == "__main__":
    unittest.main()
