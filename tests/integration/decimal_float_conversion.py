"""Directed floating-to-decimal conversion and overflow checks."""

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--linker", type=Path, required=True)
    parser.add_argument("--runtime", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    options = parser.parse_args()
    options.work_dir.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix="decimal-float-", dir=options.work_dir.resolve()))
    linker = shutil.which(str(options.linker))
    if linker is None:
        raise RuntimeError(f"linker not found: {options.linker}")
    env = os.environ.copy()
    env.update({"TEMP": str(root), "TMP": str(root), "NSC_LINKER": linker,
                "NSC_WINDOWS_RUNTIME_LIBRARY": str(options.runtime.resolve())})
    cases = Path(__file__).resolve().parent.parent / "cases" / "llvm"
    compiler = str(options.compiler.resolve())
    for name, expected in (("decimal_float_to_decimal", 0), ("decimal_float_overflow", 145)):
        case = root / name
        shutil.copytree(cases / name, case)
        project = next(case.glob("*.nsproj"))
        for optimization in ("-O0", "-O2"):
            output = case / (optimization[1:] + ".exe")
            result = subprocess.run([compiler, optimization, "--emit-project-oop-exe",
                                     str(project), "-o", str(output)],
                                    cwd=root, env=env, capture_output=True, text=True)
            if result.returncode:
                raise RuntimeError(f"{name} {optimization}: compilation failed: {result.stderr}")
            result = subprocess.run([str(output)], cwd=root, env=env, capture_output=True, text=True)
            if result.returncode != expected:
                raise RuntimeError(f"{name} {optimization}: expected {expected}, got {result.returncode}")
            print(f"PASS: {name} {optimization}: exit {expected}", flush=True)

    case = root / "decimal_float_overflow"
    source = case / "Program.ns"
    original = source.read_text()
    literal = "1000000000000000000000000000000.0"
    for name, value in (("negative", "-" + literal),
                        ("limit", "79228162514264337593543950336.0")):
        source.write_text(original.replace(literal, value))
        for optimization in ("-O0", "-O2"):
            output = case / (name + optimization[1:] + ".exe")
            result = subprocess.run([compiler, optimization, "--emit-project-oop-exe",
                                     str(case / "DecimalFloatOverflow.nsproj"), "-o", str(output)],
                                    cwd=root, env=env, capture_output=True, text=True)
            if result.returncode:
                raise RuntimeError(f"{name} {optimization}: compilation failed: {result.stderr}")
            result = subprocess.run([str(output)], cwd=root, env=env, capture_output=True, text=True)
            if result.returncode != 145:
                raise RuntimeError(f"{name} {optimization}: expected trap 145, got {result.returncode}")
            print(f"PASS: {name} {optimization}: exit 145", flush=True)


if __name__ == "__main__":
    main()
