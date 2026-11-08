import os
from pathlib import Path
import subprocess
import sys
import tempfile


def run(command, cwd):
    return subprocess.run([str(x) for x in command], cwd=cwd, capture_output=True,
                          text=True, errors='replace', timeout=120)


def main():
    compiler, fixture = map(Path, sys.argv[1:3])
    original = fixture.read_text(encoding='utf-8')
    original = original.replace('public borrowed struct View<T>', 'public borrowed struct View<lifetime a, T>')
    original = original.replace('private T target;', 'private borrowed[a] T target;')
    original = original.replace('public T Get() { return target; }',
                                'public borrowed[a] T Get() { return target; }')
    before = '''View<Payload> view;
            {
                raii Payload p = new Payload();
                Rc<Payload> strong = new Rc<Payload>(move(p));
                view = strong.Borrow();
            }
            return view.Get().Value;'''
    assert before in original
    safe = '''raii Payload p = new Payload();
            p.Value = 42;
            Rc<Payload> strong = new Rc<Payload>(move(p));
            View<Payload> view = strong.Borrow();
            return view.Get().Value;'''
    leak = '''Payload escaped;
            {
                raii Payload p = new Payload();
                Rc<Payload> strong = new Rc<Payload>(move(p));
                View<Payload> view = strong.Borrow();
                escaped = view.Get();
            }
            return escaped.Value;'''
    mismatch = '''public borrowed struct View<lifetime a, lifetime b> {
 public borrowed[a] string Source;
 public borrowed[b] string Get() { return Source; }
}
class Program { public static int Main() { return 42; } }
'''
    cases = [('safe', original.replace(before, safe), None),
             ('leak', original.replace(before, leak), 'borrowed value cannot be stored in unqualified local'),
             ('mismatch', mismatch, 'borrowed return requires a borrowed value')]
    with tempfile.TemporaryDirectory(prefix='nsc-borrowed-field-') as tmp:
        for name, source, expected_error in cases:
            folder = Path(tmp) / name
            folder.mkdir()
            (folder / 'Main.ns').write_text(source, encoding='utf-8')
            project = folder / 'Probe.nsproj'
            project.write_text('<Project><PropertyGroup><Name>Probe</Name><Language>3</Language></PropertyGroup>'
                               '<ItemGroup><Compile Include="Main.ns" /></ItemGroup></Project>', encoding='utf-8')
            result = run([compiler, '--check-project-method-bodies', project], folder)
            if expected_error:
                assert result.returncode != 0 and expected_error in result.stdout + result.stderr, (name, result.stdout, result.stderr)
                continue
            assert result.returncode == 0, result.stdout + result.stderr
            if os.name == 'nt':
                for level in ('O0', 'O2'):
                    exe = folder / (level + '.exe')
                    result = run([compiler, '--emit-project-oop-exe', project, '-o', exe,
                                  '--no-artifact-cache', '-' + level], folder)
                    assert result.returncode == 0, result.stdout + result.stderr
                    assert run([exe], folder).returncode == 42
    return 0


if __name__ == '__main__':
    sys.exit(main())
