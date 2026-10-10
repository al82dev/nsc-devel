from __future__ import annotations

import sys
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory


REPOSITORY_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPOSITORY_ROOT))

import nscbuilder3 as builder


class Builder3CatalogCsvSchemaTests(unittest.TestCase):
    def _write(self, root: Path, name: str, text: str) -> Path:
        path = root / name
        path.write_text(text, encoding="utf-8", newline="\n")
        return path

    def test_live_catalog_csv_files_satisfy_their_consumed_schema(self) -> None:
        self.assertTrue(
            builder.read_catalog_csv(builder.WAVES_PATH, builder.WAVE_CATALOG_FIELDS)
        )
        self.assertTrue(
            builder.read_catalog_csv(builder.DESIGN_PATH, builder.DESIGN_CATALOG_FIELDS)
        )
        self.assertTrue(
            builder.read_catalog_csv(
                builder.ALLOCATION_PATH, builder.ALLOCATION_CATALOG_FIELDS
            )
        )

    def test_missing_and_duplicate_headers_are_rejected_at_the_source(self) -> None:
        builder.STORAGE_ROOT.mkdir(parents=True, exist_ok=True)
        with TemporaryDirectory(dir=builder.STORAGE_ROOT) as directory:
            root = Path(directory)
            cases = {
                "missing": "case_id,component\nT-001,Host.Basic\n",
                "duplicate": "case_id,component,component\nT-001,Host.Basic,Host.Basic\n",
            }
            for name, text in cases.items():
                with self.subTest(name=name):
                    path = self._write(root, f"{name}.csv", text)
                    with self.assertRaisesRegex(ValueError, "header"):
                        builder.read_catalog_csv(
                            path, ("case_id", "component", "objective")
                        )

    def test_extra_cells_and_blank_required_values_are_rejected(self) -> None:
        builder.STORAGE_ROOT.mkdir(parents=True, exist_ok=True)
        with TemporaryDirectory(dir=builder.STORAGE_ROOT) as directory:
            root = Path(directory)
            cases = {
                "extra-cell": "case_id,component\nT-001,Host.Basic,unexpected\n",
                "blank-value": "case_id,component\nT-001,\n",
            }
            for name, text in cases.items():
                with self.subTest(name=name):
                    path = self._write(root, f"{name}.csv", text)
                    with self.assertRaisesRegex(ValueError, "row 2"):
                        builder.read_catalog_csv(path, ("case_id", "component"))

    def test_additional_named_columns_remain_forward_compatible(self) -> None:
        builder.STORAGE_ROOT.mkdir(parents=True, exist_ok=True)
        with TemporaryDirectory(dir=builder.STORAGE_ROOT) as directory:
            path = self._write(
                Path(directory),
                "extended.csv",
                "case_id,component,new_metadata\nT-001,Host.Basic,future\n",
            )
            rows = builder.read_catalog_csv(path, ("case_id", "component"))

        self.assertEqual(rows[0]["new_metadata"], "future")

    def test_wave_number_is_an_exact_positive_decimal(self) -> None:
        self.assertEqual(builder.parse_wave_number("9"), 9)
        for value in ("", "0", "01", "-1", "+1", "1.0", " 1", "1 "):
            with self.subTest(value=value):
                with self.assertRaisesRegex(ValueError, "wave"):
                    builder.parse_wave_number(value)


if __name__ == "__main__":
    unittest.main()
