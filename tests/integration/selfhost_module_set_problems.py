import subprocess
import sys
import tempfile
from pathlib import Path


def main():
    compiler = str(Path(sys.argv[1]).resolve())
    with tempfile.TemporaryDirectory(prefix="nsc-selfhost-module-problems-") as temporary:
        root = Path(temporary)
        project = root / "Test.nsproj"
        project.write_text('<Project><PropertyGroup><Name>Test</Name></PropertyGroup><ItemGroup>'
                           '<Compile Include="First.ns" /><Compile Include="Second.ns" />'
                           '</ItemGroup></Project>', encoding="utf-8")
        (root / "First.ns").write_text(
            'class First { public static int F(int x) { return x; } '
            'public static int F(bool x) { return 0; } }', encoding="utf-8")
        second = root / "Second.ns"

        def run(command):
            return subprocess.run([compiler, command, str(project)], cwd=root, capture_output=True,
                                  text=True, errors="replace", timeout=30)

        second.write_text('class Second { }', encoding="utf-8")
        for command in ("--dump-module-set", "--dump-module-exports"):
            result = run(command)
            assert result.returncode == 0, (command, result.stdout, result.stderr)

        second.write_text('class First { }', encoding="utf-8")
        result = run("--dump-module-set")
        assert result.returncode != 0 and "status=failed" in result.stdout and \
            "duplicate type across units: First" in result.stdout, (result.stdout, result.stderr)
        result = run("--dump-module-exports")
        assert result.returncode != 0 and "duplicate type across units: First" in result.stderr, \
            (result.stdout, result.stderr)
    print("selfhost module set problems: PASS")


if __name__ == "__main__":
    main()
