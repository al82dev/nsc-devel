from pathlib import Path
import re
import subprocess
import sys
import tempfile


def run(args, cwd, expected=0):
    result = subprocess.run(args, cwd=cwd, capture_output=True, text=True, errors="replace", timeout=120)
    assert result.returncode == expected, (args, result.returncode, result.stdout, result.stderr)
    return result.stdout + result.stderr


def main():
    compiler = str(Path(sys.argv[1]).resolve())
    with tempfile.TemporaryDirectory(prefix="nsc-metadata-abi-") as temporary:
        root = Path(temporary)
        producer = root / "producer"
        consumer = root / "consumer"
        producer.mkdir()
        consumer.mkdir()
        source = producer / "Producer.ns"
        source.write_text("public class Answer { public static int Value() { return 42; } }", encoding="utf-8")
        project = producer / "Producer.nsproj"
        project.write_text('<Project><PropertyGroup><Name>Producer</Name><OutputType>StaticLibrary</OutputType>'
                           '</PropertyGroup><ItemGroup><Compile Include="Producer.ns" /></ItemGroup></Project>',
                           encoding="utf-8")
        library = producer / "Producer.lib"
        metadata = producer / "Producer.nsmeta"
        build = [compiler, "-O0", "--emit-project-oop-lib", str(project), "-o", str(library)]
        run(build, root)
        first = run([compiler, "--verify-library-metadata", str(metadata)], root)
        fingerprint = re.search(r"abi-fingerprint: ([0-9a-f]{64})", first).group(1)

        (consumer / "Consumer.ns").write_text(
            "class Program { public static int Main() { return Answer.Value() == 42 ? 0 : 1; } }", encoding="utf-8")
        consumer_project = consumer / "Consumer.nsproj"
        def write_consumer(metadata_path):
            consumer_project.write_text(
                '<Project><PropertyGroup><Name>Consumer</Name><OutputType>Exe</OutputType></PropertyGroup>'
                '<ItemGroup><Compile Include="Consumer.ns" /><LibraryReference Include="' + library.as_posix() +
                '" Metadata="' + metadata_path.as_posix() + '" /></ItemGroup></Project>', encoding="utf-8")
        executable = consumer / "Consumer.exe"
        consume = [compiler, "-O0", "--emit-project-oop-exe", str(consumer_project), "-o", str(executable)]
        write_consumer(metadata)
        run(consume, root)
        run([str(executable)], root)

        corrupt = producer / "corrupt.nsmeta"
        data = bytearray(metadata.read_bytes())
        data[-1] ^= 1
        corrupt.write_bytes(data)
        write_consumer(corrupt)
        assert "checksum mismatch" in run(consume, root, expected=1)

        source.write_text("public class Answer { public static int Value() { return 42; } "
                          "public static int Other() { return 7; } }", encoding="utf-8")
        run(build, root)
        changed = run([compiler, "--verify-library-metadata", str(metadata)], root)
        assert re.search(r"abi-fingerprint: ([0-9a-f]{64})", changed).group(1) != fingerprint
    print("metadata ABI producer/consumer: PASS")


if __name__ == "__main__":
    main()
