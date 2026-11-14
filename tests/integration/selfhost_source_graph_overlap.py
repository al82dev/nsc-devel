import subprocess
import sys
import tempfile
from pathlib import Path


def main():
    compiler = str(Path(sys.argv[1]).resolve())
    with tempfile.TemporaryDirectory(prefix="nsc-selfhost-overlap-") as temporary:
        root = Path(temporary)
        (root / "Main.ns").write_text("class Program { public static int Main() { return 0; } }", encoding="utf-8")
        (root / "nested").mkdir()
        (root / "nested" / "Other.ns").write_text("class Other { }", encoding="utf-8")
        project = root / "Test.nsproj"

        def check(includes, expected_units, success):
            entries = "".join('<Compile Include="' + include + '" />' for include in includes)
            project.write_text('<Project><PropertyGroup><Name>Test</Name></PropertyGroup><ItemGroup>' +
                               entries + '</ItemGroup></Project>', encoding="utf-8")
            result = subprocess.run([compiler, "--dump-source-graph", str(project)], cwd=root,
                                    capture_output=True, text=True, errors="replace", timeout=30)
            output = result.stdout.replace("\r\n", "\n")
            assert (result.returncode == 0) == success, (includes, output, result.stderr)
            if success:
                assert "units=" + str(expected_units) + "\n" in output, (includes, output)
            else:
                assert "include matches no file" in output, (includes, output)

        check(["Main.ns", "*.ns"], 1, True)
        check(["*.ns", "Main.ns"], 1, True)
        check(["*.ns", "*.ns"], 1, True)
        check(["Main.ns", "nested/Other.ns", "**/*.ns"], 2, True)
        check(["Main.ns", "*.missing"], 0, False)
    print("selfhost source graph overlap: PASS")


if __name__ == "__main__":
    main()
