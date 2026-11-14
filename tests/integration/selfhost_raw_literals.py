import subprocess
import sys
import tempfile
from pathlib import Path


def run(args, cwd):
    return subprocess.run([str(arg) for arg in args], cwd=cwd, capture_output=True,
                          text=True, errors="replace", timeout=120)


def main():
    compiler, runtime, imports = map(lambda value: Path(value).resolve(), sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix="nsc-selfhost-raw-") as temporary:
        root = Path(temporary)
        source = root / "Raw.ns"
        source.write_text(r'''public class Program { public static int Main() {
 string raw = """a\nb""";
 if (raw.Length != 4 || raw[0] != 'a' || raw[1] != '\\' || raw[2] != 'n' || raw[3] != 'b') { return 1; }
 string wide = """"A"b"""";
 if (wide.Length != 3 || wide[0] != 'A' || wide[1] != '"' || wide[2] != 'b') { return 2; }
 string hex = """\x41""";
 if (hex.Length != 4 || hex[0] != '\\' || hex[1] != 'x' || hex[2] != '4' || hex[3] != '1') { return 5; }
 string lines = """a
b""";
 if (lines.Length != 3 || lines[1] != '\n') { return 3; }
 string ordinary = "a\nb";
 if (ordinary.Length != 3 || ordinary[1] != '\n') { return 4; }
 return 0;
} }
''', encoding="utf-8", newline="\n")
        for level in ("O0", "O2"):
            obj = root / (level + ".obj")
            exe = root / (level + ".exe")
            result = run([compiler, "--emit-object", source, "-" + level, "-o", obj], root)
            assert result.returncode == 0, (level, result.stdout, result.stderr)
            libraries = [imports / (name + ".import.lib") for name in ("KERNEL32", "KERNELBASE", "SHELL32", "UCRTBASE")]
            result = run(["lld-link", "/entry:mainCRTStartup", "/subsystem:console", "/nodefaultlib",
                          "/out:" + str(exe), obj, runtime, *libraries], root)
            assert result.returncode == 0, (level, result.stdout, result.stderr)
            result = run([exe], root)
            assert result.returncode == 0, (level, result.returncode, result.stdout, result.stderr)
            print(level + " PASS")


if __name__ == "__main__":
    main()
