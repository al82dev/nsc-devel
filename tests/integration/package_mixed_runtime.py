"""Exercise packages containing static and dynamic libraries in both manifest orders."""

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess


def run(command, cwd, environment, expected=0):
    result = subprocess.run(command, cwd=cwd, env=environment, capture_output=True, text=True)
    if result.returncode != expected:
        raise RuntimeError(f"{command!r}: exit {result.returncode}\n{result.stdout}\n{result.stderr}")
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--linker", type=Path, required=True)
    parser.add_argument("--librarian", type=Path, required=True)
    parser.add_argument("--runtime", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    options = parser.parse_args()
    compiler = str(options.compiler.resolve())
    root = options.work_dir.resolve()
    root.mkdir(parents=True, exist_ok=False)
    environment = os.environ.copy()
    environment.update({
        "TEMP": str(root),
        "TMP": str(root),
        "NSC_LINKER": str(options.linker.resolve()),
        "NSC_LLVM_LIB": str(options.librarian.resolve()),
        "NSC_WINDOWS_RUNTIME_LIBRARY": str(options.runtime.resolve()),
    })
    for name, output_type, action, output in (
        ("Static", "Library", "--emit-project-oop-lib", "Static.lib"),
        ("Dynamic", "SharedLibrary", "--emit-project-oop-dll", "Dynamic.dll"),
    ):
        directory = root / name
        directory.mkdir()
        (directory / "Main.ns").write_text(
            f"namespace Demo{name} {{ public class Api {{ public static int Get() {{ return 42; }} }} }}\n",
            encoding="utf-8",
        )
        (directory / "Library.nsproj").write_text(
            f"<Project><PropertyGroup><Name>Demo{name}</Name><OutputType>{output_type}</OutputType>"
            "</PropertyGroup><ItemGroup><Compile Include=\"Main.ns\" /></ItemGroup></Project>",
            encoding="utf-8",
        )
        run([compiler, action, str(directory / "Library.nsproj"), "-O0", "-g0", "--no-artifact-cache",
             "-o", str(directory / output)], root, environment)

    entries = [
        {"library": "Static.lib", "metadata": "Static.nsmeta"},
        {"library": "Dynamic.lib", "metadata": "Dynamic.nsmeta", "runtime": "Dynamic.dll"},
    ]
    for reverse in (False, True):
        label = "dynamic-first" if reverse else "static-first"
        package = root / ("package-" + label)
        package.mkdir()
        for name, files in (("Static", ("Static.lib", "Static.nsmeta", "Static.nsproducts")),
                            ("Dynamic", ("Dynamic.lib", "Dynamic.nsmeta", "Dynamic.nsproducts", "Dynamic.dll"))):
            for file in files:
                source = root / name / file
                if source.is_file():
                    shutil.copyfile(source, package / file)
        ordered = list(reversed(entries)) if reverse else entries
        (package / "package.json").write_text(json.dumps({
            "packageId": "Mixed", "version": "1.0", "libraries": ordered,
        }), encoding="utf-8")
        consumer = root / ("consumer-" + label)
        run([compiler, "new", "console", "-n", "Consumer", "-o", str(consumer)], root, environment)
        (consumer / "Main.ns").write_text(
            "using DemoStatic; using DemoDynamic; class Program { public static int Main() { "
            "return DemoStatic.Api.Get() + DemoDynamic.Api.Get() - 42; } }\n", encoding="utf-8")
        project = consumer / "Consumer.nsproj"
        run([compiler, "add", "package", "Mixed", "--version", "1.0", "--source", str(package),
             str(project)], root, environment)
        for optimization in ("O0", "O2"):
            run([compiler, "build", str(project), "-" + optimization], root, environment)
            run([str(consumer / "bin" / "Debug" / "Consumer.exe")], root, environment, expected=42)
            print(f"PASS: {label} {optimization}", flush=True)


if __name__ == "__main__":
    main()
