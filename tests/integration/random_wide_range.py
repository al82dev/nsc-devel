"""Random must respect the full valid int interval."""

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
    repo = Path(__file__).resolve().parent.parent.parent
    shutil.copytree(repo / "tests" / "cases" / "libraries" / "random_wide_range", root / "probe")
    for library in ("NativeSharp.Core",):
        shutil.copytree(repo / "lib" / library, root / "lib" / library,
                        ignore=shutil.ignore_patterns("obj", "bin"))
    env = os.environ.copy()
    env.update({"TEMP": str(root), "TMP": str(root), "NSC_LINKER": str(options.linker.resolve()),
                "NSC_WINDOWS_RUNTIME_LIBRARY": str(options.runtime.resolve())})
    for optimization in ("-O0", "-O2"):
        output = root / (optimization[1:] + ".exe")
        result = subprocess.run([str(options.compiler.resolve()), optimization, "--emit-project-oop-exe",
                                 str(root / "probe" / "Probe.nsproj"), "-o", str(output)],
                                cwd=root, env=env, capture_output=True, text=True)
        if result.returncode:
            raise RuntimeError(f"{optimization}: compilation failed: {result.stderr}")
        result = subprocess.run([str(output)], cwd=root, env=env, capture_output=True, text=True)
        if result.returncode:
            raise RuntimeError(f"{optimization}: program exited {result.returncode}: {result.stderr}")
        print(f"PASS: Random ranges and sequence {optimization}", flush=True)


if __name__ == "__main__":
    main()
