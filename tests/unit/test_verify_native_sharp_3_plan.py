from __future__ import annotations

import importlib.util
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).resolve().parents[2] / "tools" / "verify_native_sharp_3_plan.py"
SPEC = importlib.util.spec_from_file_location("verify_native_sharp_3_plan", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
PLAN = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PLAN)

GENERATOR_SCRIPT = Path(__file__).resolve().parents[2] / "tools" / "generate_native_sharp_3_tracking.py"
GENERATOR_SPEC = importlib.util.spec_from_file_location(
    "generate_native_sharp_3_tracking", GENERATOR_SCRIPT
)
assert GENERATOR_SPEC is not None and GENERATOR_SPEC.loader is not None
GENERATOR = importlib.util.module_from_spec(GENERATOR_SPEC)
GENERATOR_SPEC.loader.exec_module(GENERATOR)


def valid_roadmap() -> str:
    headings = "\n".join(f"## {item} — fixture" for item in sorted(PLAN.expected_nsc_feature_ids()))
    return (
        headings
        + "\n## 12. Plan integral y ordenado del toolchain\n"
        + "### 12.2 Etapa 1 — NSC\n"
        + "#### Estado ejecutivo del catálogo NSC\n"
        + "P00`–`P11`\nSH00`–`SH54`\n"
    )


def valid_tracking() -> str:
    lines = [
        "| Componente / tanda | Ítem | Implementación | Estado |",
        "|---|---|---|---|",
    ]
    for component, item_ids in PLAN.expected_component_ids().items():
        for item_id in sorted(item_ids):
            lines.append(
                f"| {component} | `{item_id}` — fixture | implementation \\| fixture | `PENDIENTE` |"
            )
    return "\n".join(lines) + "\n"


class NativeSharp3PlanTests(unittest.TestCase):
    def write(self, directory: Path, name: str, content: str) -> Path:
        path = directory / name
        path.write_text(content, encoding="utf-8")
        return path

    def test_complete_fixture_passes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            PLAN.verify_nsc_roadmap(self.write(root, "roadmap.md", valid_roadmap()))
            PLAN.verify_tracking(self.write(root, "tracking.md", valid_tracking()))

    def test_missing_feature_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            broken = valid_roadmap().replace("## NS3-T06-F08 — fixture\n", "")
            with self.assertRaisesRegex(PLAN.VerificationError, "NS3-T06-F08"):
                PLAN.verify_nsc_roadmap(self.write(root, "roadmap.md", broken))

    def test_duplicate_feature_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            broken = valid_roadmap() + "## NS3-T00-F01 — duplicate\n"
            with self.assertRaisesRegex(PLAN.VerificationError, "duplicate"):
                PLAN.verify_nsc_roadmap(self.write(root, "roadmap.md", broken))

    def test_tracking_rejects_non_table_text(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            broken = "# extra prose\n" + valid_tracking()
            with self.assertRaisesRegex(PLAN.VerificationError, "only one Markdown table"):
                PLAN.verify_tracking(self.write(root, "tracking.md", broken))

    def test_tracking_rejects_wrong_component(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            broken = valid_tracking().replace(
                "| NSC | `NS3-T00-F01`",
                "| NSLink | `NS3-T00-F01`",
            )
            with self.assertRaisesRegex(PLAN.VerificationError, "expected 'NSC'"):
                PLAN.verify_tracking(self.write(root, "tracking.md", broken))

    def test_tracking_generator_preserves_existing_states(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            tracking = self.write(
                root,
                "tracking.md",
                "| Componente / tanda | Ítem | Implementación | Estado |\n"
                "|---|---|---|---|\n"
                "| NSC | `NS3-T00-F01` — edition | work | `TERMINADA` |\n",
            )
            states = GENERATOR.read_states(tracking)
            self.assertEqual(states["NS3-T00-F01"], "TERMINADA")


if __name__ == "__main__":
    unittest.main()
