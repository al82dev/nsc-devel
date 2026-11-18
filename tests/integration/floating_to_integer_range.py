import subprocess
import sys
import tempfile
from pathlib import Path


def run(command, cwd):
    return subprocess.run([str(arg) for arg in command], cwd=cwd, capture_output=True,
                          text=True, errors="replace", timeout=120)


def main():
    compiler = Path(sys.argv[1]).resolve()
    cases = [
        ("valid", "long", "-123.75", 0, "value == -123"),
        ("overflow", "long", "1e100", 133, "value == 0"),
        ("underflow", "long", "-1e100", 133, "value == 0"),
        ("nan", "long", "0.0 / 0.0", 133, "value == 0"),
        ("positive_infinity", "long", "1.0 / 0.0", 133, "value == 0"),
        ("negative_infinity", "long", "-1.0 / 0.0", 133, "value == 0"),
        ("long_min", "long", "-9223372036854775808.0", 0, "value < 0"),
        ("int_min", "int", "-2147483648.0", 0, "value < 0"),
        ("int_over", "int", "2147483648.0", 133, "value == 0"),
        ("sbyte_fraction", "sbyte", "-128.75", 0, "value < 0"),
        ("sbyte_below", "sbyte", "-129.0", 133, "value == 0"),
        ("byte_fraction", "byte", "-0.75", 0, "value == 0"),
        ("byte_below", "byte", "-1.0", 133, "value == 0"),
    ]
    cases = [(name, "double", target, literal, expected, check)
             for name, target, literal, expected, check in cases]
    cases += [
        ("float_int_min", "float", "int", "(float)-2147483648.0", 0, "value < 0"),
        ("float_int_over", "float", "int", "(float)2147483648.0", 133, "value == 0"),
        ("float_sbyte_fraction", "float", "sbyte", "(float)-128.75", 0, "value < 0"),
    ]
    with tempfile.TemporaryDirectory(prefix="nsc-fptoi-") as temporary:
        root = Path(temporary)
        project = root / "Test.nsproj"
        source = root / "Main.ns"
        project.write_text('<Project><PropertyGroup><Name>Test</Name><OutputType>Exe</OutputType>'
                           '</PropertyGroup><ItemGroup><Compile Include="Main.ns" /></ItemGroup></Project>',
                           encoding="utf-8")
        failures = []
        for level in ("O0", "O2"):
            for name, input_type, target, literal, expected, check in cases:
                source.write_text('public class Program { public static ' + target +
                                  ' Convert(' + input_type + ' input) { return (' + target + ')input; } '
                                  'public static int Main() { ' + target + ' value = Convert(' + literal +
                                  '); return ' + check + ' ? 0 : 1; } }', encoding="utf-8")
                executable = root / (name + level + ".exe")
                result = run([compiler, "-" + level, "--emit-project-oop-exe", project,
                              "-o", executable], root)
                if result.returncode:
                    failures.append((level, name, "compile", result.stdout, result.stderr))
                    continue
                result = run([executable], root)
                print(level, name, "exit", result.returncode, "expected", expected, flush=True)
                if result.returncode != expected:
                    failures.append((level, name, result.returncode, expected))
        assert not failures, failures


if __name__ == "__main__":
    main()
