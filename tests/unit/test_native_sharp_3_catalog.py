from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path


TOOLS = Path(__file__).resolve().parents[2] / "tools"
sys.path.insert(0, str(TOOLS))
import native_sharp_3_catalog as catalog  # noqa: E402
import verify_native_sharp_3_catalog as verifier  # noqa: E402


def fixture_items() -> list[catalog.CatalogItem]:
    return [
        catalog.CatalogItem(component, item_id, "title", "contract", "PENDIENTE", "source")
        for component, item_ids in verifier.expected_ids_by_component().items()
        for item_id in sorted(item_ids)
    ]


def fixture_roadmap() -> str:
    patterns = {
        "NSC": "## {item} — fixture",
        "NSLink": "#### {item} — fixture",
        "NSDBG": "#### {item} — fixture",
        "NSC-LSP": "#### {item} — fixture",
        "NSC-VSCode": "#### {item} — fixture",
        "NSASM": "#### {item} — fixture",
    }
    expected = verifier.expected_ids_by_component()
    return "\n".join(
        patterns[component].format(item=item_id)
        for component in patterns
        for item_id in sorted(expected[component])
    ) + "\n"


class NativeSharp3CatalogueTests(unittest.TestCase):
    def test_catalogue_round_trip_and_tracking_are_exact(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            master = root / "master.md"
            master.write_text("# master\n", encoding="utf-8")
            items = fixture_items()
            catalog.replace_catalogue(master, items, allow_create=True)
            parsed = catalog.parse_catalogue(master)
            self.assertEqual(len(parsed), 308)
            self.assertEqual(parsed, sorted(
                items,
                key=lambda value: (
                    catalog.COMPONENT_ORDER.get(value.component, 999), value.item_id
                ),
            ))
            tracking = root / "tracking.md"
            tracking.write_text(catalog.render_tracking(parsed), encoding="utf-8")
            verifier.verify_tracking(tracking, parsed)

    def test_update_state_changes_master_not_an_independent_tracker(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            master = root / "master.md"
            master.write_text("# master\n", encoding="utf-8")
            catalog.replace_catalogue(master, fixture_items(), allow_create=True)
            catalog.update_item(master, "NS3-LNK-01", "EN CURSO", "directed audit")
            row = {item.item_id: item for item in catalog.parse_catalogue(master)}["NS3-LNK-01"]
            self.assertEqual(row.state, "EN CURSO")
            self.assertEqual(row.evidence, "directed audit")

    def test_catalogue_preserves_rows_with_empty_evidence(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            master = Path(temporary) / "master.md"
            master.write_text("# master\n", encoding="utf-8")
            items = fixture_items()
            items[0] = items[0]._replace(evidence="")
            catalog.replace_catalogue(master, items, allow_create=True)
            parsed = catalog.parse_catalogue(master)
            self.assertEqual(len(parsed), 308)
            self.assertEqual(parsed[0].evidence, "")

    def test_terminal_state_requires_evidence(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            master = Path(temporary) / "master.md"
            master.write_text("# master\n", encoding="utf-8")
            catalog.replace_catalogue(master, fixture_items(), allow_create=True)
            with self.assertRaisesRegex(RuntimeError, "requires evidence"):
                catalog.update_item(master, "NS3-LNK-01", "TERMINADA", "")

    def test_roadmap_detects_a_missing_component_item(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            roadmap = Path(temporary) / "roadmap.md"
            roadmap.write_text(
                fixture_roadmap().replace("#### NS3-LSP-10 — fixture\n", ""),
                encoding="utf-8",
            )
            with self.assertRaisesRegex(verifier.VerificationError, "NS3-LSP-10"):
                verifier.verify_roadmap(roadmap)


if __name__ == "__main__":
    unittest.main()
