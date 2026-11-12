"""Check generic delegate return types across a NativeSharp library boundary."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile


PRODUCER = """namespace DelegateFixture {
 public delegate T Reducer<T>(T left, T right);
 public static class Operations {
  public static int AddInt(int left, int right) { return left + right; }
  public static long AddLong(long left, long right) { return left + right; }
 }
}"""
CONSUMER = """using DelegateFixture;
int Main() {
 raii Reducer<int> addInt = Operations.AddInt;
 raii Reducer<long> addLong = Operations.AddLong;
 return addInt(1, 2) == 3 && addLong(4L, 5L) == 9L ? 0 : 1;
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
    with tempfile.TemporaryDirectory(prefix="nsc-generic-delegate-binary-") as temporary:
        root = Path(temporary)
        (root / "Producer.ns").write_text(PRODUCER, encoding="utf-8")
        (root / "Main.ns").write_text(CONSUMER, encoding="utf-8")
        producer = root / "Producer.nsproj"
        producer.write_text(
            '<Project><PropertyGroup><Name>DelegateFixture</Name>'
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
                '<Project><PropertyGroup><Name>DelegateConsumer</Name></PropertyGroup>'
                '<ItemGroup><Compile Include="Main.ns" /><LibraryReference Include="' +
                library.as_posix() + '" Metadata="' + metadata.as_posix() +
                '" /></ItemGroup></Project>',
                encoding="utf-8")
            assert run(
                [compiler, "--check-project-method-bodies", consumer], root).returncode == 0
            for consumer_level in ("O0", "O2"):
                executable = root / (producer_level + "-" + consumer_level + ".exe")
                result = run(
                    [compiler, "--emit-project-oop-exe", consumer,
                     "-" + consumer_level, "-o", executable,
                     "--no-artifact-cache", "--no-error-log"],
                    root)
                assert result.returncode == 0
                assert run([executable], root).returncode == 0
    return 0


if __name__ == "__main__":
    sys.exit(main())
