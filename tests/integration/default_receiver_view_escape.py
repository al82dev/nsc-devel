"""Reject views borrowed from default-initialized temporary struct receivers."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile


def run(command, cwd):
    result = subprocess.run([str(x) for x in command], cwd=cwd, capture_output=True,
                            text=True, errors='replace', timeout=120)
    return result.returncode, result.stdout + result.stderr


def main():
    compiler = Path(sys.argv[1]).resolve()
    types = '''borrowed struct View<lifetime a, T> {
 private borrowed[a] T* target;
 public View(ref[a] T value) { unsafe("view") { target = &value; } }
 public ref[this] T Get() { unsafe("view") { return ref *target; } }
}
struct Owner { public int Value; public View<int> Borrow() { return new View<int>(ref Value); } }
'''
    cases = [
        ('return', 'public static View<int> Leak() { return (default(Owner)).Borrow(); }', False),
        ('local', 'public static View<int> Leak() { View<int> v = (default(Owner)).Borrow(); return v; }', False),
        ('ref', '''public static View<int> Borrow(ref Owner owner) { return owner.Borrow(); }
 public static int Main() { Owner owner; owner.Value = 42; View<int> v = Borrow(ref owner); return v.Get(); }''', True),
    ]
    with tempfile.TemporaryDirectory(prefix='default-receiver-view-') as temporary:
        for name, body, valid in cases:
            root = Path(temporary) / name
            root.mkdir()
            (root / 'Main.ns').write_text(types + 'class Program { ' + body + ' }', encoding='utf-8')
            project = root / 'Probe.nsproj'
            project.write_text('<Project><PropertyGroup><Name>Probe</Name><Language>3</Language>'
                '</PropertyGroup><ItemGroup><Compile Include="Main.ns" /></ItemGroup></Project>', encoding='utf-8')
            code, output = run([compiler, '--check-project-method-bodies', project, '--no-banner'], root)
            if valid:
                if code: raise AssertionError(f'{name}: {output}')
                if os.name == 'nt':
                    for level in ('O0', 'O2'):
                        exe = root / (level + '.exe')
                        code, output = run([compiler, '--emit-project-oop-exe', project, '-' + level,
                                            '-o', exe, '--no-artifact-cache'], root)
                        if code: raise AssertionError(f'{name} {level}: {output}')
                        code, output = run([exe], root)
                        if code != 42: raise AssertionError(f'{name} {level}: exit {code}\n{output}')
                        print(name, level, 'exit=42', flush=True)
            elif code == 0 or 'temporary receiver' not in output:
                raise AssertionError(f'{name}: expected temporary receiver rejection, got {code}\n{output}')
            else:
                print(name, 'rejected as expected', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
