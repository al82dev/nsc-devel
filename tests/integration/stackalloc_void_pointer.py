import subprocess
import sys
import tempfile
from pathlib import Path


def run(args, cwd):
    return subprocess.run(args, cwd=cwd, capture_output=True, text=True, errors="replace", timeout=120)


def main():
    compiler = str(Path(sys.argv[1]).resolve())
    with tempfile.TemporaryDirectory(prefix="nsc-void-pointer-") as temporary:
        root = Path(temporary)
        project = root / "Probe.nsproj"
        project.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup>'
                           '<ItemGroup><Compile Include="Main.ns" /></ItemGroup></Project>', encoding="utf-8")
        source = root / "Main.ns"
        source.write_text('int Main() { int value = 42; void** p = stackalloc void*[2]; '
                          'p[0] = &value; int* result = (int*)p[0]; return *result == 42 ? 0 : 1; }',
                          encoding="utf-8")
        for level in ("O0", "O2"):
            executable = root / (level + ".exe")
            build = run([compiler, "--emit-project-oop-exe", str(project), "-" + level,
                         "-o", str(executable), "--no-artifact-cache"], root)
            assert build.returncode == 0, build.stdout + build.stderr
            result = run([str(executable)], root)
            assert result.returncode == 0, (level, result.returncode, result.stdout, result.stderr)
        source.write_text('int Main() { void* value = stackalloc void[1]; return 0; }', encoding="utf-8")
        invalid = run([compiler, "--check-project-method-bodies", str(project)], root)
        assert invalid.returncode != 0 and "void cannot be used as an allocation element type" in invalid.stderr, (
            invalid.returncode, invalid.stdout, invalid.stderr)
    print("stackalloc void-pointer: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
