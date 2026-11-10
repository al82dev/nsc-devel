"""Check calls on owners of borrowed fields against the binary SDK."""
from pathlib import Path
import subprocess
import sys
import tempfile


def run(command, cwd):
    return subprocess.run([str(part) for part in command], cwd=cwd,
                          capture_output=True, text=True, errors='replace')


def main():
    compiler, library = map(lambda value: Path(value).resolve(), sys.argv[1:3])
    metadata = library.with_suffix('.nsmeta')
    assert library.is_file() and metadata.is_file()
    with tempfile.TemporaryDirectory(prefix='nsc-field-method-borrow-') as temporary:
        root = Path(temporary)
        project = root / 'Consumer.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Consumer</Name><Language>3</Language>'
                           '</PropertyGroup><ItemGroup><Compile Include="Program.ns" />'
                           '<LibraryReference Include="' + library.as_posix() + '" Metadata="' +
                           metadata.as_posix() + '" /></ItemGroup></Project>', encoding='utf-8')
        source = root / 'Program.ns'
        for family in ('Rc', 'Arc'):
            common = '''using NativeSharp.Core;
class Payload { public long Data; public Payload() { Data = 42l; } }
class Holder { public STRONG<Payload> value;
 public void Replace() { value = SharedPointers.MAKE<Payload>(); } }
'''.replace('STRONG', family).replace('MAKE', 'Make' + family)
            bodies = {
                'unsafe': '''Holder holder = new Holder(); holder.value = SharedPointers.MAKE<Payload>();
 SharedView<Payload> view = holder.value.Borrow(); holder.Replace(); return (int)view.Get().Data;''',
                'alias': '''Holder holder = new Holder(); holder.value = SharedPointers.MAKE<Payload>();
 borrowed Holder alias = holder; SharedView<Payload> view = holder.value.Borrow();
 alias.Replace(); return (int)view.Get().Data;''',
                'independent': '''Holder first = new Holder(); Holder second = new Holder();
 first.value = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = first.value.Borrow();
 second.Replace(); return view.Get().Data == 42l ? 42 : 1;''',
                'after-scope': '''Holder holder = new Holder(); holder.value = SharedPointers.MAKE<Payload>();
 { SharedView<Payload> view = holder.value.Borrow(); if (view.Get().Data != 42l) return 1; }
 holder.Replace(); return holder.value.Get().Data == 42l ? 42 : 2;''',
            }
            for name, body in bodies.items():
                source.write_text(common + 'class Program { public static int Main() { ' +
                                  body.replace('MAKE', 'Make' + family) + ' } }', encoding='utf-8')
                result = run([compiler, '--check-project-method-bodies', project], root)
                output = result.stdout + result.stderr
                if name in ('unsafe', 'alias'):
                    assert result.returncode != 0 and 'borrow' in output, (family, name, output)
                    print(family, name, 'rejected', flush=True)
                    continue
                assert result.returncode == 0, (family, name, output)
                for level in ('O0', 'O2'):
                    executable = root / (family + '-' + name + '-' + level + '.exe')
                    result = run([compiler, '--emit-project-oop-exe', project, '-' + level,
                                  '-o', executable, '--no-artifact-cache'], root)
                    assert result.returncode == 0, (family, name, level, result.stdout + result.stderr)
                    result = run([executable], root)
                    assert result.returncode == 42, (family, name, level, result.returncode)
                    print(family, name, level, 'exit=42', flush=True)


if __name__ == '__main__':
    main()
