import os
from pathlib import Path
import subprocess
import sys
import tempfile


def run(command, cwd):
    return subprocess.run([str(value) for value in command], cwd=cwd, capture_output=True,
                          text=True, errors='replace', timeout=120)


def main():
    compiler = Path(sys.argv[1])
    types = '''borrowed struct View<lifetime a, T> {
 private borrowed[a] T* target;
 public View(ref[a] T value) { unsafe("view") { target = &value; } }
 public ref[this] T Get() { unsafe("view") { return ref *target; } }
}
struct Payload { public long Value; ~Payload() { } }
'''
    safe = '''class Program { public static int Main() { Payload p; p.Value = 42l;
 View<Payload> view = new View<Payload>(ref p); return (int)view.Get().Value; } }
'''
    scope = '''class Program { public static int Main() { View<Payload> view;
 { Payload p; p.Value = 42l; view = new View<Payload>(ref p); }
 return (int)view.Get().Value; } }
'''
    leak = '''class Program { public static ref Payload Leak() {
 Payload p; View<Payload> view = new View<Payload>(ref p); return ref view.Get(); }
 public static int Main() { return 42; } }
'''
    view_return = '''class Program { public static View<Payload> LeakView() {
 Payload p; return new View<Payload>(ref p); }
 public static int Main() { return 42; } }
'''
    move = '''class Program { public static int Main() { Payload p;
 View<Payload> view = new View<Payload>(ref p); Payload other = move(p); return (int)view.Get().Value; } }
'''
    multi_types = types.replace('public View(ref[a] T value) { unsafe("view") { target = &value; } }',
                                'public View(ref[a] T x, ref[a] T y) { unsafe("view") { target = &y; } }')
    multi = '''class Program { public static int Main() { Payload outer; View<Payload> view;
 { Payload inner; view = new View<Payload>(ref outer, ref inner); }
 return (int)view.Get().Value; } }
'''
    cases = [('safe', types + safe, None),
             ('scope', types + scope, "view borrowed from local 'p' cannot be stored in a longer-lived local"),
             ('return', types + leak, 'ref return cannot expose storage owned by the current stack frame'),
             ('view-return', types + view_return, 'cannot be returned'),
             ('move', types + move, 'cannot move owner'),
             ('multiple', multi_types + multi, "view borrowed from local 'inner' cannot be stored in a longer-lived local")]
    with tempfile.TemporaryDirectory(prefix='nsc-shared-value-view-') as temporary:
        for name, source, expected_error in cases:
            root = Path(temporary) / name
            root.mkdir()
            (root / 'Main.ns').write_text(source, encoding='utf-8')
            project = root / 'Probe.nsproj'
            project.write_text('<Project><PropertyGroup><Name>Probe</Name><Language>3</Language></PropertyGroup>'
                               '<ItemGroup><Compile Include="Main.ns" /></ItemGroup></Project>', encoding='utf-8')
            result = run([compiler, '--check-project-method-bodies', project, '--no-banner'], root)
            output = result.stdout + result.stderr
            if expected_error:
                assert result.returncode != 0 and expected_error in output, (name, output)
            else:
                assert result.returncode == 0, output
                if os.name == 'nt':
                    for level in ('O0', 'O2'):
                        exe = root / (level + '.exe')
                        result = run([compiler, '--emit-project-oop-exe', project, '-o', exe,
                                      '--no-artifact-cache', '-' + level], root)
                        assert result.returncode == 0, result.stdout + result.stderr
                        result = run([exe], root)
                        assert result.returncode == 42, (level, result.returncode)
    return 0


if __name__ == '__main__':
    sys.exit(main())
