import os
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    compiler, fixture = map(Path, sys.argv[1:3])
    source = fixture.read_text(encoding='utf-8')
    old = '''View<Payload> view;
            {
                raii Payload p = new Payload();
                Rc<Payload> strong = new Rc<Payload>(move(p));
                view = strong.Borrow();
            }
            return view.Get().Value;'''
    assert old in source
    safe = '''raii Payload p = new Payload();
            p.Value = 42;
            Rc<Payload> strong = new Rc<Payload>(move(p));
            View<Payload> view = strong.Borrow();
            return view.Get().Value;'''
    returning = source.replace('public static int Main()', 'public static View<Payload> Leak()')
    returning = returning.replace(old, '''raii Payload p = new Payload();
            Rc<Payload> strong = new Rc<Payload>(move(p));
            return strong.Borrow();''')
    returning = returning.replace('public static View<Payload> Leak()',
                                  'public static int Main() { return 42; }\n        public static View<Payload> Leak()')
    moving = source.replace(old, safe.replace('return view.Get().Value;',
                            'Rc<Payload> other = move(strong); return view.Get().Value;'))
    temporary = source.replace(old, '''raii Payload p = new Payload();
            View<Payload> view = (new Rc<Payload>(move(p))).Borrow();
            return view.Get().Value;''')
    factory = source.replace('public static int Main()',
                             'public static Rc<Payload> Make(owned Payload p) { return new Rc<Payload>(move(p)); }\n        public static int Main()')
    factory = factory.replace(old, '''raii Payload p = new Payload();
            View<Payload> view = Make(move(p)).Borrow();
            return view.Get().Value;''')
    chain = source.replace('public T Get() { return target; }',
                           'public T Get() { return target; } public View<T> Identity() { return this; }')
    chain = chain.replace(old, safe.replace('strong.Borrow()', 'strong.Borrow().Identity()'))
    cases = [('escape', source, 'cannot be stored in a longer-lived local'),
             ('return', returning, 'cannot be returned'),
             ('move', moving, 'cannot move owner'),
             ('temporary', temporary, 'view cannot borrow from a temporary receiver'),
             ('factory', factory, 'view cannot borrow from a temporary receiver'),
             ('chain', chain, None),
             ('safe', source.replace(old, safe), None)]
    with tempfile.TemporaryDirectory(prefix='nsc-view-lifetime-') as tmp:
        for name, text, expected_error in cases:
            folder = Path(tmp) / name
            folder.mkdir()
            (folder / 'Main.ns').write_text(text, encoding='utf-8')
            project = folder / 'Probe.nsproj'
            project.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup>'
                               '<ItemGroup><Compile Include="Main.ns" /></ItemGroup></Project>', encoding='utf-8')
            cmd = [str(compiler), '--check-project-method-bodies', str(project)]
            result = subprocess.run(cmd, capture_output=True, text=True, errors='replace', timeout=120)
            output = result.stdout + result.stderr
            if expected_error:
                assert result.returncode != 0 and expected_error in output, (name, output)
            else:
                assert result.returncode == 0, output
                if os.name == 'nt':
                    exe = folder / 'safe.exe'
                    result = subprocess.run([str(compiler), '--emit-project-oop-exe', str(project), '-o', str(exe),
                                             '--no-artifact-cache'], capture_output=True, text=True,
                                            errors='replace', timeout=120)
                    assert result.returncode == 0, result.stdout + result.stderr
                    result = subprocess.run([str(exe)], capture_output=True, timeout=60)
                    assert result.returncode == 42, result.returncode
    return 0


if __name__ == '__main__':
    sys.exit(main())
