import subprocess
import sys
import tempfile
from pathlib import Path


def run(args, cwd):
    return subprocess.run([str(arg) for arg in args], cwd=cwd, capture_output=True,
                          text=True, errors="replace", timeout=120)


def main():
    compiler, runtime, imports = map(lambda value: Path(value).resolve(), sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix="nsc-selfhost-exists-") as temporary:
        root = Path(temporary)
        source = root / "Probe.ns"
        source.write_text('''public class File { public static bool Exists(string path) { return false; } }
public class Directory { public static bool Exists(string path) { return false; } }
public class Program { public static int Main() {
 if (File.Exists(".")) { return 1; }
 if (!Directory.Exists(".")) { return 2; }
 if (!File.Exists("Probe.ns")) { return 3; }
 if (Directory.Exists("Probe.ns")) { return 4; }
 if (File.Exists("missing")) { return 5; }
 if (Directory.Exists("missing")) { return 6; }
 return 0; } }
''', encoding="utf-8")
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
    return 0


if __name__ == "__main__":
    sys.exit(main())
