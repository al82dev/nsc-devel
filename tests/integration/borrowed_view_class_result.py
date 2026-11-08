import os
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    compiler, fixture = map(Path, sys.argv[1:3])
    source = fixture.read_text(encoding='utf-8')
    start = source.index('        public static int Main()')
    source = source[:start]
    setup = '''raii Payload p = new Payload(); p.Value = 42;
        Rc<Payload> strong = new Rc<Payload>(move(p)); View<Payload> view = strong.Borrow();'''
    cases = {
        'direct': (setup + ' return view.Get().Value;', None),
        'alias': (setup + ' borrowed Payload alias = view.Get(); return alias.Value;', None),
        'local_escape': ('Payload escaped; { ' + setup + ' escaped = view.Get(); } return escaped.Value;', 'borrowed value cannot'),
        'field_escape': (setup + ' Saved = view.Get(); return 0;', 'borrowed value cannot'),
        'argument_escape': (setup + ' Store(view.Get()); return 0;', 'borrowed argument requires'),
        'move': (setup + ' borrowed Payload alias = view.Get(); Rc<Payload> other = move(strong); return alias.Value;', 'cannot move owner'),
    }
    helpers = 'public static Payload Saved; public static void Store(Payload p) { Saved = p; }\n'
    with tempfile.TemporaryDirectory(prefix='nsc-view-class-result-') as tmp:
        folder = Path(tmp)
        project = folder / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup>'
                           '<ItemGroup><Compile Include="Main.ns" /></ItemGroup></Project>', encoding='utf-8')
        for return_mode in ('value', 'readonly'):
            variant = source
            if return_mode == 'readonly':
                variant = variant.replace('public T Get() { return target; }',
                                          'public ref readonly T Get() { return ref target; }')
            for name, (body, expected_error) in cases.items():
                (folder / 'Main.ns').write_text(variant + helpers + 'public static int Main() { ' + body + ' } } }', encoding='utf-8')
                result = subprocess.run([str(compiler), '--check-project-method-bodies', str(project)],
                                        capture_output=True, text=True, errors='replace', timeout=120)
                output = result.stdout + result.stderr
                if expected_error:
                    assert result.returncode != 0 and expected_error in output, (return_mode, name, output)
                else:
                    assert result.returncode == 0, (return_mode, name, output)
                    if os.name == 'nt':
                        for level in ('O0', 'O2'):
                            exe = folder / (name + return_mode + level + '.exe')
                            result = subprocess.run([str(compiler), '--emit-project-oop-exe', str(project), '-o', str(exe),
                                                     '--no-artifact-cache', '-' + level], capture_output=True, text=True,
                                                    errors='replace', timeout=120)
                            assert result.returncode == 0, (return_mode, name, level, result.stdout + result.stderr)
                            result = subprocess.run([str(exe)], capture_output=True, timeout=60)
                            assert result.returncode == 42, (return_mode, name, level, result.returncode)
            for qualifier in ('', 'borrowed '):
                text = variant + helpers + 'public static ' + qualifier + 'Payload Leak() { ' + setup + ' return view.Get(); } public static int Main() { return 0; } } }'
                (folder / 'Main.ns').write_text(text, encoding='utf-8')
                result = subprocess.run([str(compiler), '--check-project-method-bodies', str(project)],
                                        capture_output=True, text=True, errors='replace', timeout=120)
                assert result.returncode != 0 and 'borrowed' in result.stdout + result.stderr, (return_mode, qualifier, result.stdout + result.stderr)
    return 0


if __name__ == '__main__':
    sys.exit(main())
