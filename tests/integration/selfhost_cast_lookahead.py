import subprocess
import sys
import tempfile
from pathlib import Path


def run(args, cwd):
    return subprocess.run([str(arg) for arg in args], cwd=cwd, capture_output=True,
                          text=True, errors="replace", timeout=120)


def main():
    compiler, runtime, imports = map(lambda value: Path(value).resolve(), sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix="nsc-selfhost-cast-") as temporary:
        root = Path(temporary)
        source = root / "Main.ns"
        source.write_text('class Foo { } class Program { public static int Main() { '
                          'int Foo = 1; return (Foo < 2) ? (int)0 : 1; } }', encoding="utf-8")
        ast = run([compiler, "--dump-ast", source], root)
        assert ast.returncode == 0, (ast.stdout, ast.stderr)
        assert "BinaryExpression op=Less" in ast.stdout and "CastExpression" in ast.stdout, ast.stdout
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
        source.write_text('class Program { public static int Main() { return (int 2); } }', encoding="utf-8")
        malformed = run([compiler, "--dump-ast", source], root)
        assert malformed.returncode != 0, malformed.stdout
    print("selfhost cast lookahead: PASS")


if __name__ == "__main__":
    main()
