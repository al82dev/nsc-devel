import subprocess
import sys
import tempfile
from pathlib import Path


def run(args, cwd):
    return subprocess.run([str(arg) for arg in args], cwd=cwd, capture_output=True,
                          text=True, errors="replace", timeout=120)


def main():
    compiler = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="nsc-main-args-") as temporary:
        root = Path(temporary)
        project = root / "Test.nsproj"
        project.write_text('<Project><PropertyGroup><Name>Test</Name><OutputType>Exe</OutputType>'
                           '</PropertyGroup><ItemGroup><Compile Include="Main.ns" /></ItemGroup></Project>',
                           encoding="utf-8")
        source = root / "Main.ns"
        source.write_text('public class Program { public static int Main(string[] args) { '
                          'if (args.Length == 0) { return 0; } '
                          'if (args.Length == 1) { return args[0] == "ñ" ? 0 : 2; } '
                          'if (args.Length != 3 || args[0] != "alpha" || '
                          'args[1] != "two words" || args[2] != "") { return 1; } '
                          'args[0] = "replacement"; return 0; } }', encoding="utf-8")
        for level in ("O0", "O2"):
            executable = root / (level + ".exe")
            result = run([compiler, "-" + level, "--emit-project-oop-exe", project, "-o", executable], root)
            assert result.returncode == 0, (level, result.stdout, result.stderr)
            for arguments in ([], ["ñ"], ["alpha", "two words", ""]):
                result = run([executable, *arguments], root)
                assert result.returncode == 0, (level, arguments, result.returncode, result.stdout, result.stderr)
            print(level + " PASS")

        source.write_text('using NativeSharp.Core; namespace NativeSharp.Core { public class Exception { } } '
                          'public class Program { public static int Main(string[] args) throws Exception { '
                          'throw new Exception(); } }', encoding="utf-8")
        panic_exe = root / "panic.exe"
        result = run([compiler, "-O0", "--emit-project-oop-exe", project, "-o", panic_exe], root)
        assert result.returncode == 0, (result.stdout, result.stderr)
        result = run([panic_exe, "original"], root)
        # An uncaught NativeSharp exception retains the process's existing SEH exit code.
        assert result.returncode == 0xE04E5301, (result.returncode, result.stdout, result.stderr)

        source.write_text('public class Program { public static int Main(string[] args) { '
                          'args[0] = "replacement"; panic(); return 0; } }', encoding="utf-8")
        result = run([compiler, "-O0", "--emit-project-oop-exe", project, "-o", panic_exe], root)
        assert result.returncode == 0, (result.stdout, result.stderr)
        result = run([panic_exe, "original"], root)
        assert result.returncode == 142, (result.returncode, result.stdout, result.stderr)

        source.write_text('public class Program { public static int Main() { return 0; } }', encoding="utf-8")
        no_args = root / "no-args.exe"
        result = run([compiler, "-O0", "--emit-project-oop-exe", project, "-o", no_args], root)
        assert result.returncode == 0, (result.stdout, result.stderr)
        assert run([no_args, "ignored"], root).returncode == 0
        for declaration in (
            'public class Program { public static int Main(int[] args) { return 0; } }',
            'public class Program { public static int Main(int arg) { return 0; } }',
            'public class Program { public static int Main() { return 0; } '
            'public static int Main(string[] args) { return 0; } }',
        ):
            source.write_text(declaration, encoding="utf-8")
            result = run([compiler, "-O0", "--emit-project-oop-exe", project,
                          "-o", root / "invalid.exe"], root)
            assert result.returncode != 0, (declaration, result.stdout, result.stderr)


if __name__ == "__main__":
    main()
