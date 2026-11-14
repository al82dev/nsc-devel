"""Directed tests for exact wide decimal literals."""

import argparse
import os
from pathlib import Path
import shutil
import subprocess


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--linker", type=Path, required=True)
    parser.add_argument("--runtime", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    options = parser.parse_args()
    root = options.work_dir.resolve()
    root.mkdir(parents=True, exist_ok=False)
    case = root / "case"
    shutil.copytree(Path(__file__).resolve().parent.parent / "cases" / "llvm" /
                    "decimal_literal_wide", case)
    env = os.environ.copy()
    env.update({"TEMP": str(root), "TMP": str(root), "NSC_LINKER": str(options.linker.resolve()),
                "NSC_WINDOWS_RUNTIME_LIBRARY": str(options.runtime.resolve())})
    compiler = str(options.compiler.resolve())
    project = case / "DecimalLiteralWide.nsproj"
    source = case / "Program.ns"
    original = source.read_text()
    maximum = "79228162514264337593543950335m"
    result = subprocess.run([compiler, "--dump-project-oop-llvm", str(project)],
                            cwd=root, env=env, capture_output=True, text=True)
    encoded_trailing = str((4 << 96) + 12300)
    if result.returncode or encoded_trailing not in result.stdout:
        raise RuntimeError("static literal 1.2300 did not preserve its coefficient and scale")
    print("PASS: trailing zero coefficient and scale are preserved", flush=True)
    for scenario, text, success in (
        ("valid", original, True),
        ("too-wide", original.replace(maximum, "79228162514264337593543950336m"), False),
        ("exponent-overflow", original.replace(maximum, "1e29m"), False),
        ("scale-overflow", original.replace(maximum, "0.00000000000000000000000000001m"), False),
        ("coefficient-trailing-zero", original.replace(maximum, "792281625142643375935439503350e-1m"), False),
    ):
        source.write_text(text)
        for optimization in ("-O0", "-O2"):
            executable = case / (scenario + optimization[1:] + ".exe")
            result = subprocess.run([compiler, optimization, "--emit-project-oop-exe",
                                     str(project), "-o", str(executable)],
                                    cwd=root, env=env, capture_output=True, text=True)
            if success:
                if result.returncode:
                    raise RuntimeError(f"{scenario} {optimization}: compilation failed: {result.stderr}")
                run = subprocess.run([str(executable)], cwd=root, env=env, capture_output=True, text=True)
                if run.returncode:
                    raise RuntimeError(f"{scenario} {optimization}: program exited {run.returncode}")
            elif result.returncode == 0 or (
                "decimal literal coefficient or scale is out of range" not in result.stderr and
                "decimal const coefficient or scale is out of range" not in result.stderr
            ):
                raise RuntimeError(f"{scenario} {optimization}: unexpected result: {result.stderr}")
            print(f"PASS: {scenario} {optimization}", flush=True)


if __name__ == "__main__":
    main()
