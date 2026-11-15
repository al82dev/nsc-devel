"""FindAll must evaluate each predicate once per input element."""

import argparse
import os
from pathlib import Path
import re
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
    shutil.copytree(repo / "tests" / "cases" / "libraries" / "list_findall_effects", root / "probe")
    for library in ("NativeSharp.Core", "NativeSharp.Core.Collections.Generic"):
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
        print(f"PASS: FindAll predicates and controls {optimization}", flush=True)

    project = root / "probe" / "Probe.nsproj"
    project.write_text(project.read_text(encoding="utf-8").replace(
        "<OutputType>Exe</OutputType>",
        "<OutputType>Exe</OutputType><PanicProfile>unwind-native</PanicProfile>"),
        encoding="utf-8")
    result = subprocess.run([str(options.compiler.resolve()), "--dump-project-oop-llvm", str(project)],
                            cwd=root, env=env, capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(f"unwind IR failed: {result.stderr}")
    functions = [body for body in result.stdout.split("\ndefine ")
                 if "__FindAll__" in body.partition("\n")[0]
                 and "__int__" in body.partition("\n")[0]]
    if len(functions) != 2:
        raise RuntimeError(f"expected both int FindAll overloads, got {len(functions)}")
    for body in functions:
        unwind = re.search(r"%delegate\.static\.result = invoke[^\n]*\n\s*to label %[^ ]+ unwind label %(eh\.cleanup\.[\w.]+)", body)
        if not unwind:
            raise RuntimeError("delegate call has no unwind cleanup")
        label = unwind.group(1)
        if not re.search(r"(?m)^" + re.escape(label) + r":.*\n\s*%eh\.cleanuppad[\w.]* = cleanuppad", body):
            raise RuntimeError("delegate unwind target is not a cleanup")
        if "call void @__nativesharp_free(ptr %raii.owner.cleanup)" not in body:
            raise RuntimeError("borrowed temporary is not freed on unwind")
        if not re.search(r"cleanupret from %eh\.cleanuppad[\w.]* unwind label %eh\.finally\.catch", body):
            raise RuntimeError("temporary cleanup does not continue through the FindAll finally block")
        if not re.search(r"(?s)eh\.finally\.catch\.body\.[\w.]+:.*?sub i32 %field\.load[\w.]*, 1\n\s*store i32 %sub[\w.]*, ptr %field\.addr[\w.]*.*?call void @__nativesharp_throw", body):
            raise RuntimeError("FindAll does not restore its mutation guard before rethrowing")
    print("PASS: both FindAll temporary buffers have an unwind cleanup", flush=True)


if __name__ == "__main__":
    main()
