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
        'atomic': ('class Holder { public static Arc<Payload> value; public static WeakArc<Payload> weak; }', False),
        'local': ('class Program { public static int Main() { Rc<Payload> value; return 42; } }', False),
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
            (root / 'Main.ns').write_text('using NativeSharp.Core; class Payload { } ' + body + program,
                                          encoding='utf-8')
            command = [str(compiler), '--check-project-method-bodies', str(project)]
            result = subprocess.run(command, cwd=root, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            valid = (result.returncode != 0 and 'thread-local shared pointer' in result.stdout
                     if rejected else result.returncode == 0)
            assert valid, (name, result.returncode, result.stdout)
            print(name, 'exit=', result.returncode, 'command=', ' '.join(command), flush=True)
            print(result.stdout, flush=True)


if __name__ == '__main__':
    main()
