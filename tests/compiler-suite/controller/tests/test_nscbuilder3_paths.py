from __future__ import annotations

import hashlib
import sys
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory


REPOSITORY_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPOSITORY_ROOT))

import nscbuilder3 as builder


class Builder3PathTests(unittest.TestCase):
    def test_every_writable_root_must_remain_on_a_drive(self) -> None:
        baseline = builder.BuilderConfig(jobs=1)
        writable_fields = ("build_root", "compiler_build_root", "install_prefix")

        for field in writable_fields:
            with self.subTest(field=field):
                values = {**baseline.__dict__, field: f"C:/forbidden/{field}"}
                with self.assertRaisesRegex(ValueError, "must be on A:"):
                    builder.BuilderConfig(**values).validated(require_tools=False)

    def test_jobs_outside_supported_range_are_rejected(self) -> None:
        for jobs in (0, 257):
            with self.subTest(jobs=jobs):
                with self.assertRaisesRegex(ValueError, "between 1 and 256"):
                    builder.BuilderConfig(jobs=jobs).validated(require_tools=False)

    def test_file_receipt_identifies_exact_selected_file(self) -> None:
        builder.STORAGE_ROOT.mkdir(parents=True, exist_ok=True)
        with TemporaryDirectory(dir=builder.STORAGE_ROOT) as directory:
            artifact = Path(directory) / "tool.exe"
            payload = b"selected-tool-identity\n"
            artifact.write_bytes(payload)

            receipt = builder.file_receipt("tool", artifact)

            self.assertEqual(receipt["name"], "tool")
            self.assertEqual(receipt["path"], str(artifact.resolve()))
            self.assertEqual(receipt["size"], len(payload))
            self.assertEqual(receipt["sha256"], hashlib.sha256(payload).hexdigest())

    def test_compiler_receipt_preserves_selected_nsc_and_nslink(self) -> None:
        builder.STORAGE_ROOT.mkdir(parents=True, exist_ok=True)
        with TemporaryDirectory(dir=builder.STORAGE_ROOT) as directory:
            root = Path(directory)
            nsc = root / "selected-nsc.exe"
            nslink = root / "selected-nslink.exe"
            nsc.write_bytes(b"nsc-selected\n")
            nslink.write_bytes(b"nslink-selected\n")
            config = builder.BuilderConfig(
                nsc_executable=str(nsc),
                nslink_executable=str(nslink),
                compiler_build_root="A:/nsc-devel/build-clang",
                install_prefix="A:/native-sharp-3.0-baseline",
                build_root="A:/nsc-devel/build-compiler-suite-v3",
                jobs=1,
            )

            receipt = builder.compiler_receipt(config)

            self.assertEqual(
                [(item["name"], item["path"]) for item in receipt["tools"]],
                [("nsc", str(nsc.resolve())), ("nslink", str(nslink.resolve()))],
            )


if __name__ == "__main__":
    unittest.main()
