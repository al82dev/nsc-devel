import subprocess
import sys
import tempfile
from pathlib import Path


def main():
    compiler = str(Path(sys.argv[1]).resolve())
    with tempfile.TemporaryDirectory(prefix="nsc-selfhost-name-") as temporary:
        root = Path(temporary)
        source = root / "Main.ns"

        def check(text, success):
            source.write_text(text, encoding="utf-8")
            result = subprocess.run([compiler, "--dump-sema", str(source)], cwd=root,
                                    capture_output=True, text=True, errors="replace", timeout=30)
            assert (result.returncode == 0) == success, (text, result.stdout, result.stderr)
            if not success:
                assert "unresolved identifier: missing" in result.stderr, result.stderr

        check("class Program { public static int Main() { return missing; } }", False)
        check("class Program { public static int Main() { missing; return 0; } }", False)
        check("class Program { public static int Id(int x) { return x; } "
              "public static int Main() { int value = 4; return Id(value); } }", True)
        check("class Item { public static int Value() { return 4; } } "
              "class Program { public static int Main() { return Item.Value(); } }", True)
        (root / "Item.ns").write_text("class Item { public static int Value() { return 4; } }", encoding="utf-8")
        source.write_text("class Program { public static int Main() { return Item.Value(); } }", encoding="utf-8")
        project = root / "Test.nsproj"
        project.write_text('<Project><PropertyGroup><Name>Test</Name></PropertyGroup><ItemGroup>'
                           '<Compile Include="Main.ns" /><Compile Include="Item.ns" />'
                           '</ItemGroup></Project>', encoding="utf-8")
        result = subprocess.run([compiler, "--dump-module-set", str(project)], cwd=root,
                                capture_output=True, text=True, errors="replace", timeout=30)
        assert result.returncode == 0 and "unresolved identifier" not in result.stderr, \
            (result.stdout, result.stderr)
    print("selfhost unresolved identifiers: PASS")


if __name__ == "__main__":
    main()
