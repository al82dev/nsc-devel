"""Check a non-generic struct trait impl across a NativeSharp library boundary."""
from pathlib import Path
import os
import re
import subprocess
import sys
import tempfile


PRODUCER = """namespace StructTraitFixture {
 public trait HasValue { int Value(); }
 public struct Number { public int value; }
 impl HasValue for Number { int Value() { return this.value; } }
}"""
CONSUMER = """using StructTraitFixture;
class Program {
 private static int Read<T>(T item) where T : HasValue { return item.Value(); }
 public static int Main() { Number item; item.value = 42;
  return item.Value() + Read<Number>(item) - 42; }
}"""


def run(command, root):
    args = [str(item) for item in command]
    result = subprocess.run(
        args, cwd=root, capture_output=True, text=True, errors="replace", timeout=120)
    print("COMMAND", subprocess.list2cmdline(args), "EXIT", result.returncode,
          result.stdout + result.stderr, flush=True)
    return result


def main():
    if os.name != "nt":
        raise SystemExit("native Windows execution required")
    compiler = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="nsc-struct-trait-binary-") as temporary:
        root = Path(temporary)
        (root / "Producer.ns").write_text(PRODUCER, encoding="utf-8")
        (root / "Main.ns").write_text(CONSUMER, encoding="utf-8")
        producer = root / "Producer.nsproj"
        producer.write_text(
            '<Project><PropertyGroup><Name>StructTraitFixture</Name>'
            '<OutputType>StaticLibrary</OutputType></PropertyGroup>'
            '<ItemGroup><Compile Include="Producer.ns" /></ItemGroup></Project>',
            encoding="utf-8")
        consumer = root / "Consumer.nsproj"
        for producer_level in ("O0", "O2"):
            library = root / ("producer-" + producer_level + ".lib")
            result = run(
                [compiler, "--emit-project-oop-lib", producer, "-" + producer_level,
                 "-o", library, "--no-artifact-cache", "--no-error-log"],
                root)
            assert result.returncode == 0
            metadata = library.with_suffix(".nsmeta")
            assert metadata.is_file()
            consumer.write_text(
                '<Project><PropertyGroup><Name>StructTraitConsumer</Name></PropertyGroup>'
                '<ItemGroup><Compile Include="Main.ns" /><LibraryReference Include="' +
                library.as_posix() + '" Metadata="' + metadata.as_posix() +
                '" /></ItemGroup></Project>',
                encoding="utf-8")
            result = run([compiler, "--dump-project-oop-mir", consumer], root)
            assert result.returncode == 0
            direct = re.findall(
                r"direct-instance-call[^\r\n]*Number__Value", result.stdout)
            assert len(direct) >= 2
            assert "interface-instance-call" not in result.stdout
            assert "virtual-instance-call" not in result.stdout
            assert "allocate-and-construct" not in result.stdout
            for consumer_level in ("O0", "O2"):
                executable = root / (producer_level + "-" + consumer_level + ".exe")
                result = run(
                    [compiler, "--emit-project-oop-exe", consumer,
                     "-" + consumer_level, "-o", executable,
                     "--no-artifact-cache", "--no-error-log"],
                    root)
                assert result.returncode == 0
                assert run([executable], root).returncode == 42
    return 0


if __name__ == "__main__":
    sys.exit(main())
