"""`return move(owner)` while a borrow of the owner is alive (docs/fixes/return-move-con-vista.md).

EN: Every local of the callable ends with the return, so a borrow of the owner cannot be used after a move
    that is the whole value of the return; the move is accepted. A move outside a return, followed by a use of
    the borrow, is still rejected. Accepted cases are built and run at O0, O1, O2 and O3 (exit 42).
ES: Todo local del callable termina con el return, asi que un prestamo del dueno no puede usarse despues de un
    move que es todo el valor del return; el move se acepta. Un move fuera de un return seguido de un uso del
    prestamo se sigue rechazando. Los casos aceptados se compilan y ejecutan en O0..O3 (salida 42).
Usage: return_move_with_view.py <nsc.exe> <NativeSharpCore.lib>
"""
from pathlib import Path
import subprocess
import sys
import tempfile

COMMON = '''using NativeSharp.Core;
class Payload { public long Data; public Payload() { Data = 42l; } }
class Holder { public Rc<Payload> value; }
'''

CASES = {
    'return-move-with-alias': (True, '''public static owned Holder Make() { raii Holder owner = new Holder();
 owner.value = SharedPointers.MakeRc<Payload>(); borrowed Holder alias = owner;
 if (alias.value.Get().Data != 42l) { return null; } return move(owner); }''',
        'raii Holder made = Make(); return made.value.Get().Data == 42l ? 42 : 1;'),
    'return-move-with-view': (True, '''public static owned Holder Make() { raii Holder owner = new Holder();
 owner.value = SharedPointers.MakeRc<Payload>(); SharedView<Payload> view = owner.value.Borrow();
 if (view.Get().Data != 42l) { return null; } return move(owner); }''',
        'raii Holder made = Make(); return made.value.Get().Data == 42l ? 42 : 1;'),
    'move-then-alias-use': (False, '''public static int Make() { raii Holder owner = new Holder();
 owner.value = SharedPointers.MakeRc<Payload>(); borrowed Holder alias = owner;
 raii Holder other = move(owner); return (int)alias.value.Get().Data; }''',
        'return Make();'),
}


def run(command, cwd):
    return subprocess.run([str(part) for part in command], cwd=cwd,
                          capture_output=True, text=True, errors='replace')


def main():
    compiler, library = map(lambda value: Path(value).resolve(), sys.argv[1:3])
    metadata = library.with_suffix('.nsmeta')
    assert library.is_file() and metadata.is_file()
    with tempfile.TemporaryDirectory(prefix='nsc-return-move-view-') as temporary:
        root = Path(temporary)
        project = root / 'Consumer.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Consumer</Name>'
                           '</PropertyGroup><ItemGroup><Compile Include="Program.ns" />'
                           '<LibraryReference Include="' + library.as_posix() + '" Metadata="' +
                           metadata.as_posix() + '" /></ItemGroup></Project>', encoding='utf-8')
        source = root / 'Program.ns'
        for name, (accepted, helper, body) in CASES.items():
            source.write_text(COMMON + 'class Program { ' + helper + ' public static int Main() { ' + body +
                              ' } }', encoding='utf-8')
            result = run([compiler, '--check-project-method-bodies', project], root)
            output = result.stdout + result.stderr
            if not accepted:
                assert result.returncode != 0 and 'borrow' in output, (name, output)
                print(name, 'rejected', flush=True)
                continue
            assert result.returncode == 0, (name, output)
            for level in ('O0', 'O1', 'O2', 'O3'):
                executable = root / (name + '-' + level + '.exe')
                result = run([compiler, '--emit-project-oop-exe', project, '-' + level,
                              '-o', executable, '--no-artifact-cache'], root)
                assert result.returncode == 0, (name, level, result.stdout + result.stderr)
                result = run([executable], root)
                assert result.returncode == 42, (name, level, result.returncode)
                print(name, level, 'exit=42', flush=True)


if __name__ == '__main__':
    main()
