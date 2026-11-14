import subprocess
import sys
import tempfile
from pathlib import Path


def run(compiler, *args):
    return subprocess.run([str(compiler), *map(str, args)], capture_output=True,
                          text=True, errors="replace", timeout=30)


def main():
    compiler = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="nsc-selfhost-metadata-") as temporary:
        root = Path(temporary)
        (root / "Main.ns").write_text("class Program { public static int Main() { return 0; } }", encoding="utf-8")
        project = root / "Probe.nsproj"
        project.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup><ItemGroup>'
                           '<Compile Include="Main.ns"/></ItemGroup></Project>', encoding="utf-8")
        metadata = root / "Probe.nsmeta"
        emitted = run(compiler, "--emit-nsmeta", project, metadata)
        assert emitted.returncode == 0, (emitted.stdout, emitted.stderr)
        content = metadata.read_text(encoding="utf-8")
        assert content.startswith("NSMETA\n") and "\nname=Probe\n" in content, content
        assert run(compiler, "--verify-nsmeta", metadata).returncode == 0
        metadata.write_text(content.replace("NSMETA\n", "NSMETA/arbitrary-label\n", 1), encoding="utf-8")
        assert run(compiler, "--verify-nsmeta", metadata).returncode == 0
        metadata.write_text("NSMETA\nname=\n", encoding="utf-8")
        assert run(compiler, "--verify-nsmeta", metadata).returncode != 0
        metadata.write_text("unrelated\nname=Probe\n", encoding="utf-8")
        assert run(compiler, "--verify-nsmeta", metadata).returncode != 0
    print("selfhost metadata content: PASS")


if __name__ == "__main__":
    main()
