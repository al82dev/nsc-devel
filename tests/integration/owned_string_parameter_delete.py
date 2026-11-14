import subprocess
import sys
import tempfile
from pathlib import Path


def run(command, cwd):
    return subprocess.run([str(arg) for arg in command], cwd=cwd, capture_output=True,
                          text=True, errors="replace", timeout=120)


def main():
    compiler = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="nsc-owned-string-delete-") as temporary:
        root = Path(temporary)
        project = root / "Test.nsproj"
        source = root / "Main.ns"
        project.write_text('<Project><PropertyGroup><Name>Test</Name><OutputType>Exe</OutputType>'
                           '</PropertyGroup><ItemGroup><Compile Include="Main.ns" /></ItemGroup></Project>',
                           encoding="utf-8")
        valid = '''using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception { } }
public class Program {
 public static void Consume(owned string value, bool drop) { if (drop) { delete value; return; } }
 public static void Fail(owned string value) throws Exception { delete value; throw new Exception(); }
 public static int Main() { Consume("abc" + "def", true); Consume("hello" + "world", false);
   try { Fail("abc" + "def"); return 1; } catch (Exception error) { return 0; } }
}'''
        invalid = ['''public class Program {
 public static void Consume(owned string value) { delete value; delete value; }
 public static int Main() { return 0; }
}''', '''public class Program {
 public static int Consume(owned string value) { delete value; return value.Length; }
 public static int Main() { return 0; }
}''']
        executable = root / "Test.exe"
        for level in ("O0", "O2"):
            source.write_text(valid, encoding="utf-8")
            result = run([compiler, "-" + level, "--emit-project-oop-exe", project, "-o", executable], root)
            assert result.returncode == 0, (level, result.stdout, result.stderr)
            result = run([executable], root)
            assert result.returncode == 0, (level, result.returncode, result.stdout, result.stderr)
            for case in invalid:
                source.write_text(case, encoding="utf-8")
                result = run([compiler, "-" + level, "--emit-project-oop-exe", project, "-o", executable], root)
                assert result.returncode != 0 and "NS3020" in result.stderr and "value" in result.stderr, (level, result.stdout, result.stderr)
            print(level + " PASS", flush=True)


if __name__ == "__main__":
    main()
