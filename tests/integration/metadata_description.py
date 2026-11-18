import subprocess
import sys
import tempfile
from pathlib import Path


def main():
    compiler = str(Path(sys.argv[1]).resolve())
    with tempfile.TemporaryDirectory(prefix="nsc-metadata-description-") as temporary:
        root = Path(temporary)
        project = root / "Library.nsproj"
        project.write_text('<Project><PropertyGroup><Name>Library</Name><OutputType>Library</OutputType>'
                           '</PropertyGroup><ItemGroup><Compile Include="Library.ns" /></ItemGroup></Project>',
                           encoding="utf-8")
        (root / "Library.ns").write_text('public class Example { public static int Value() { return 42; } }',
                                         encoding="utf-8")
        metadata = root / "Library.nsmeta"
        for args in (["--emit-project-library-metadata", str(project), "-o", str(metadata), "--verbose"],
                     ["--verify-library-metadata", str(metadata)]):
            result = subprocess.run([compiler, *args], cwd=root, capture_output=True,
                                    text=True, errors="replace", timeout=120)
            assert result.returncode == 0, (args, result.stdout, result.stderr)
            assert "format: NSMETA\n" in result.stdout.replace("\r\n", "\n"), result.stdout
            assert "format: NSMETA " not in result.stdout, result.stdout
    print("metadata description: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
