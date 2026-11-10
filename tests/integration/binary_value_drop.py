"""A precompiled value must retain its producer-owned destruction contract."""
from pathlib import Path
import subprocess
import sys
import tempfile


def run(command, cwd):
    command = [str(value) for value in command]
    result = subprocess.run(command, cwd=cwd, capture_output=True, text=True,
                            errors='replace', timeout=120)
    print('COMMAND', subprocess.list2cmdline(command), 'EXIT', result.returncode,
          '\n', result.stdout, result.stderr, flush=True)
    return result


def main():
    compiler = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix='nsc-binary-value-drop-') as temporary:
        root = Path(temporary)
        (root / 'Types.ns').write_text('''public class State { public static int Trace; }
public struct Tracked {
 public int Id;
 public Tracked(int id) { Id = id; State.Trace = State.Trace * 10 + id; }
 ~Tracked() { State.Trace = State.Trace * 10 + Id + 3; }
}
class Part {
 public int Id;
 public Part(int id) { Id = id; State.Trace = State.Trace * 10 + id; }
 ~Part() { State.Trace = State.Trace * 10 + Id + 3; }
}
public struct Owner {
 private owned Part part;
 public Owner(int id) { part = new Part(id); }
}
public struct Plain { public int Value; }
''', encoding='utf-8')
        producer = root / 'Producer.nsproj'
        producer.write_text('<Project><PropertyGroup><Name>ValueDropProducer</Name>'
                           '<OutputType>StaticLibrary</OutputType></PropertyGroup><ItemGroup>'
                           '<Compile Include="Types.ns"/></ItemGroup></Project>', encoding='utf-8')
        library = root / 'ValueDropProducer.lib'
        result = run([compiler, '--emit-project-oop-lib', producer, '-O0', '-o', library,
                      '--no-artifact-cache'], root)
        if result.returncode or not library.with_suffix('.nsmeta').is_file():
            raise AssertionError('producer or metadata failed')
        project = root / 'Consumer.nsproj'
        project.write_text('<Project><PropertyGroup><Name>ValueDropConsumer</Name></PropertyGroup><ItemGroup>'
                           '<Compile Include="Main.ns"/><LibraryReference Include="' + library.as_posix() +
                           '" Metadata="' + library.with_suffix('.nsmeta').as_posix() +
                           '"/></ItemGroup></Project>', encoding='utf-8')
        cases = {
            'custom-destructor': '{ Tracked a = new Tracked(1); Tracked b = new Tracked(2); }',
            'private-owned-field': '{ Owner a = new Owner(1); Owner b = new Owner(2); }',
        }
        for name, body in cases.items():
            (root / 'Main.ns').write_text('int Main() { ' + body +
                ' return State.Trace == 1254 ? 42 : 1; }', encoding='utf-8')
            for level in ('O0', 'O2'):
                executable = root / (name + '-' + level + '.exe')
                built = run([compiler, '--emit-project-oop-exe', project, '-' + level,
                             '-o', executable, '--no-artifact-cache'], root)
                if built.returncode:
                    raise AssertionError((name, level, 'compile'))
                executed = run([executable], root)
                if executed.returncode != 42:
                    raise AssertionError((name, level, 'execute', executed.returncode))
        checks = {
            'plain-copy': ('Plain a = new Plain(); Plain b = a;', True),
            'destructor-copy': ('Tracked a = new Tracked(1); Tracked b = a;', False),
            'owned-field-copy': ('Owner a = new Owner(1); Owner b = a;', False),
        }
        for name, (body, valid) in checks.items():
            (root / 'Main.ns').write_text('int Main() { ' + body + ' return 42; }', encoding='utf-8')
            result = run([compiler, '--check-project-method-bodies', project, '--no-banner'], root)
            if (result.returncode == 0) != valid or (
                    not valid and 'move-only value' not in result.stdout + result.stderr):
                raise AssertionError((name, result.returncode, result.stdout, result.stderr))
    print('binary_value_drop: PASS', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
