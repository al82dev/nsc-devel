import subprocess
import sys
import tempfile
from pathlib import Path


def run(args, cwd):
    return subprocess.run([str(arg) for arg in args], cwd=cwd, capture_output=True,
                          text=True, errors="replace", timeout=120)


def main():
    compiler, runtime, imports = map(lambda value: Path(value).resolve(), sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix="nsc-selfhost-raw-comptime-") as temporary:
        root = Path(temporary)
        source = root / "Main.ns"
        source.write_text(r'''class Program { public static int Main() {
 string first = """a"#run(1+1)b""";
 if (first[2] != '#') { return 1; }
 string wide = """"x""#run(3+4)y"""";
 if (wide[3] != '#') { return 2; }
 string slash = """a\"#run(3+4)b""";
 if (slash[3] != '#') { return 5; }
 string word = """a"comptime b""";
 if (word[2] != 'c') { return 6; }
 string normal = "a\"#run(3+4)b";
 if (normal[2] != '#') { return 3; }
 return #run(1+1) == 2 ? 0 : 4;
} }
''', encoding="utf-8", newline="\n")
        libraries = [imports / (name + ".import.lib") for name in ("KERNEL32", "KERNELBASE", "SHELL32", "UCRTBASE")]
        for level in ("O0", "O2"):
            obj = root / (level + ".obj")
            exe = root / (level + ".exe")
            result = run([compiler, "--emit-object", source, "-" + level, "-o", obj], root)
            assert result.returncode == 0, (level, result.stdout, result.stderr)
            result = run(["lld-link", "/entry:mainCRTStartup", "/subsystem:console", "/nodefaultlib",
                          "/out:" + str(exe), obj, runtime, *libraries], root)
            assert result.returncode == 0, (level, result.stdout, result.stderr)
            result = run([exe], root)
            assert result.returncode == 0, (level, result.returncode, result.stdout, result.stderr)
            print(level + " PASS")


if __name__ == "__main__":
    main()
