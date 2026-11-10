"""Focused layout checks for tuple fields containing value structures."""
from pathlib import Path
import subprocess
import sys
import tempfile


def run(command, cwd):
    return subprocess.run([str(value) for value in command], cwd=cwd, capture_output=True,
                          text=True, errors='replace', timeout=120)


def main():
    compiler = Path(sys.argv[1]).resolve()
    cases = {
        'later-element': '''struct AOuter { public (ZInner, int) Value; }
struct ZInner { public long Value; }
class Program { public static int Main() { ZInner inner = { 40l }; AOuter outer = { (inner, 2) };
 var (value, n) = outer.Value; return sizeof(AOuter) == 16 ? (int)value.Value + n : 1; } }''',
        'earlier-element': '''struct ZOuter { public (AInner, int) Value; }
struct AInner { public long Value; }
class Program { public static int Main() { AInner inner = { 40l }; ZOuter outer = { (inner, 2) };
 var (value, n) = outer.Value; return sizeof(ZOuter) == 16 ? (int)value.Value + n : 1; } }''',
        'nested-tuple': '''struct AOuter { public ((ZInner, int), int) Value; }
struct ZInner { public long Value; }
class Program { public static int Main() { ZInner inner = { 40l }; AOuter outer = { ((inner, 2), 0) };
 var ((value, n), zero) = outer.Value; return (int)value.Value + n + zero; } }''',
        'direct-control': '''struct AOuter { public ZInner Value; }
struct ZInner { public long Value; }
class Program { public static int Main() { ZInner inner = { 42l }; AOuter outer = { inner };
 return (int)outer.Value.Value; } }''',
        'pointer-cycle': '''struct AOuter { public (AOuter*, int) Value; }
class Program { public static int Main() { AOuter value = default(AOuter);
 return sizeof(AOuter) == 16 ? 42 : 1; } }''',
    }
    cycles = {
        'direct-cycle': '''struct AOuter { public (AOuter, int) Value; }
class Program { public static int Main() { return 42; } }''',
        'indirect-cycle': '''struct AOuter { public (ZInner, int) Value; }
struct ZInner { public AOuter Value; }
class Program { public static int Main() { return 42; } }''',
    }
    failures = []
    with tempfile.TemporaryDirectory(prefix='tuple-field-layout-') as temporary:
        root = Path(temporary)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name><Language>3</Language>'
                           '</PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
                           '</ItemGroup></Project>', encoding='utf-8')
        for name, source in cases.items():
            (root / 'Main.ns').write_text(source, encoding='utf-8')
            for level in ('O0', 'O2'):
                executable = root / (name + '-' + level + '.exe')
                command = [compiler, '--emit-project-oop-exe', project, '-' + level,
                           '-o', executable, '--no-artifact-cache']
                result = run(command, root)
                print(name, level, 'compile=' + str(result.returncode), flush=True)
                if result.returncode != 0:
                    failures.append((name, level, result.stdout + result.stderr))
                    continue
                result = run([executable], root)
                print(name, level, 'run=' + str(result.returncode), flush=True)
                if result.returncode != 42:
                    failures.append((name, level, result.stdout + result.stderr))
        for name, source in cycles.items():
            (root / 'Main.ns').write_text(source, encoding='utf-8')
            result = run([compiler, '--check-project-layouts', project], root)
            output = result.stdout + result.stderr
            print(name, 'rejected=' + str(result.returncode != 0), flush=True)
            if result.returncode == 0 or 'recursive by-value struct' not in output:
                failures.append((name, result.returncode, output))
    for failure in failures:
        print('FAIL', *failure, file=sys.stderr)
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
