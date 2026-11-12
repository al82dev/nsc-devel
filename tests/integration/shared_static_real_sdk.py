import subprocess
import sys
import tempfile
from pathlib import Path


def main():
    compiler = Path(sys.argv[1]).resolve()
    library = Path(sys.argv[2]).resolve()
    metadata = library.with_suffix('.nsmeta')
    assert library.is_file() and metadata.is_file(), (library, metadata)
    cases = {
        'rc': ('class Holder { public static Rc<Payload> value; }', True),
        'weak_rc': ('class Holder { public static WeakRc<Payload> value; }', True),
        'rc_nested': ('struct Box { public Rc<Payload> value; } class Holder { public static Box value; }', True),
        'weak_array': ('class Holder { public static WeakRc<Payload>[] value; }', True),
        'atomic_with_local': ('class Holder { public static Arc<WeakRc<Payload>> value; }', True),
        'chained_tuple_local': ('class Holder { public static Other value; }', True),
        'chained_nested_tuple_local': ('class Holder { public static Other value; }', True),
        'chained_tuple_namespace': ('namespace Store { Other value; }', True),
        'chained_tuple_atomic': ('class Holder { public static Other value; }', False),
        'atomic': ('class Holder { public static Arc<Payload> value; public static WeakArc<Payload> weak; }', False),
        'local': ('class Program { public static int Main() { Rc<Payload> value; return 42; } }', False),
    }
    aliases = {
        'chained_tuple_local': 'using Pair = (int, WeakRc<Payload>); using Other = Pair; ',
        'chained_nested_tuple_local': 'using Pair = (int, (int, WeakRc<Payload>)); using Other = Pair; ',
        'chained_tuple_namespace': 'using Pair = (int, WeakRc<Payload>); using Other = Pair; ',
        'chained_tuple_atomic': 'using Pair = (int, WeakArc<Payload>); using Other = Pair; ',
    }
    with tempfile.TemporaryDirectory(prefix='nsc-real-sdk-static-') as temporary:
        root = Path(temporary)
        project = root / 'Consumer.nsproj'
        project.write_text('<Project><PropertyGroup><Name>StaticSdkConsumer</Name><Language>3</Language>'
                           '</PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
                           '<LibraryReference Include="' + library.as_posix() + '" Metadata="' +
                           metadata.as_posix() + '" /></ItemGroup></Project>', encoding='utf-8')
        for name, (body, rejected) in cases.items():
            program = '' if name == 'local' else 'class Program { public static int Main() { return 42; } }'
            (root / 'Main.ns').write_text('using NativeSharp.Core; ' + aliases.get(name, '') +
                                          'class Payload { } ' + body + program,
                                          encoding='utf-8')
            command = [str(compiler), '--check-project-method-bodies', str(project)]
            result = subprocess.run(command, cwd=root, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            valid = (result.returncode != 0 and 'thread-local shared pointer' in result.stdout
                     if rejected else result.returncode == 0)
            assert valid, (name, result.returncode, result.stdout)
            print(name, 'exit=', result.returncode, 'command=', ' '.join(command), flush=True)
            print(result.stdout, flush=True)
            if name == 'chained_tuple_atomic':
                for level in ('O0', 'O2'):
                    executable = root / ('atomic-alias-' + level + '.exe')
                    command = [str(compiler), '--emit-project-oop-exe', str(project),
                               '-' + level, '-o', str(executable)]
                    result = subprocess.run(command, cwd=root, text=True, stdout=subprocess.PIPE,
                                            stderr=subprocess.STDOUT)
                    print(name, level, 'build exit=', result.returncode,
                          'command=', ' '.join(command), result.stdout, flush=True)
                    assert result.returncode == 0, (name, level, result.stdout)
                    result = subprocess.run([str(executable)], cwd=root, text=True,
                                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                    print(name, level, 'run exit=', result.returncode, result.stdout, flush=True)
                    assert result.returncode == 42, (name, level, result.returncode, result.stdout)


if __name__ == '__main__':
    main()
