"""Check ThinLTO warnings against effective project, profile and command-line optimization."""

import argparse
import os
from pathlib import Path
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
    environment = os.environ.copy()
    environment.update({"TEMP": str(root), "TMP": str(root), "NSC_LINKER": str(options.linker.resolve()),
                        "NSC_WINDOWS_RUNTIME_LIBRARY": str(options.runtime.resolve())})
    (root / "Program.ns").write_text(
        "class Program { public static int Main() { return 42; } }\n", encoding="utf-8")
    cases = (
        ("project-o0", "O0", (), "O0"),
        ("project-o2", "O2", (), "O2"),
        ("cli-o0", "O2", ("-O0",), "O0"),
        ("cli-o2", "O0", ("-O2",), "O2"),
        ("profile-debug", "O2", ("--profile=debug",), "O0"),
        ("profile-release", "O0", ("--profile=release",), "O2"),
    )
    for name, project_level, override, effective in cases:
        project = root / (name + ".nsproj")
        project.write_text(
            f"<Project><PropertyGroup><Name>WarningTest</Name><OptimizationLevel>{project_level}"
            "</OptimizationLevel></PropertyGroup><ItemGroup><Compile Include=\"Program.ns\" />"
            "</ItemGroup></Project>", encoding="utf-8")
        executable = root / (name + ".exe")
        command = [str(options.compiler.resolve()), "--emit-project-oop-exe", str(project),
                   "-flto=thin", "-g0", "--no-artifact-cache", "--verbose", *override,
                   "-o", str(executable)]
        result = subprocess.run(command, cwd=root, env=environment, capture_output=True, text=True)
        warning = "-flto=thin at -O0 builds the ordinary partitioned code"
        if result.returncode != 0 or (warning in result.stderr) != (effective == "O0") or (
                "optimization: " + effective) not in result.stdout:
            raise RuntimeError(f"{name}: exit {result.returncode}\n{result.stdout}\n{result.stderr}")
        executed = subprocess.run([str(executable)], cwd=root, env=environment)
        if executed.returncode != 42:
            raise RuntimeError(f"{name}: executable returned {executed.returncode}")
        print(f"PASS: {name} ({effective})", flush=True)


if __name__ == "__main__":
    main()
