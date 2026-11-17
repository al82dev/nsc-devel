"""Storage-preserving callees while a borrowed field view is alive
(docs/fixes/llamadas-preservadoras-de-almacenamiento.md).

EN: A source callee proven not to release storage (reads, scalar writes, calls to other preserving callees,
    cycles included) is accepted while a view of the receiver's field is alive; a callee that replaces the
    owned field is still rejected, directly, through a chain and inside a cycle, as receiver and as
    argument. A program that does not hold a view is unchanged. Accepted cases run at O0..O3 (exit 42).
ES: Un callee de fuente que se demuestra que no libera almacenamiento (lecturas, escrituras escalares,
    llamadas a otros callees que preservan, con ciclos) se acepta mientras vive una vista del campo del
    receptor; un callee que reemplaza el campo owned se sigue rechazando, directo, por cadena y dentro de un
    ciclo, como receptor y como argumento. Un programa sin vista no cambia. Los aceptados corren en O0..O3.
Usage: storage_preserving_callee.py <nsc.exe> <NativeSharpCore.lib>
"""
from pathlib import Path
import subprocess
import sys
import tempfile

COMMON = '''using NativeSharp.Core;
class Payload { public long Data; public Payload() { Data = 42l; } }
class Holder { public Rc<Payload> value; private int counter;
 public int Count() { return counter; }
 public void Touch() { counter = counter + 1; }
 public int CountChain() { return Count(); }
 public int Ping(int n) { if (n <= 0) { return counter; } return Pong(n - 1); }
 public int Pong(int n) { return Ping(n); }
 public void Replace() { value = SharedPointers.MakeRc<Payload>(); }
 public void ReplaceChain() { Replace(); }
 public int RePing(int n) { if (n <= 0) { Replace(); return 0; } return RePong(n - 1); }
 public int RePong(int n) { return RePing(n); } }
'''

HELPERS = '''class Program {
 public static int Peek(Holder item) { return item.Count(); }
 public static void ReplaceValue(Holder item) { item.ReplaceChain(); }
 public static int Main() { Holder holder = new Holder(); holder.value = SharedPointers.MakeRc<Payload>();
'''

VIEW = 'SharedView<Payload> view = holder.value.Borrow(); '
CHECK = ' return view.Get().Data == 42l ? 42 : 1; } }'

CASES = {
    'preserving-read': (True, VIEW + 'int seen = holder.Count(); if (seen != 0) { return 2; }'),
    'preserving-scalar-write': (True, VIEW + 'holder.Touch(); if (holder.Count() != 1) { return 2; }'),
    'preserving-chain': (True, VIEW + 'if (holder.CountChain() != 0) { return 2; }'),
    'preserving-cycle': (True, VIEW + 'if (holder.Ping(3) != 0) { return 2; }'),
    'preserving-argument': (True, VIEW + 'if (Peek(holder) != 0) { return 2; }'),
    'no-view': (True, 'holder.Replace(); SharedView<Payload> view = holder.value.Borrow();'),
    'mutating-direct': (False, VIEW + 'holder.Replace();'),
    'mutating-chain': (False, VIEW + 'holder.ReplaceChain();'),
    'mutating-cycle': (False, VIEW + 'holder.RePing(3);'),
    'mutating-argument': (False, VIEW + 'ReplaceValue(holder);'),
}


def run(command, cwd):
    return subprocess.run([str(part) for part in command], cwd=cwd,
                          capture_output=True, text=True, errors='replace')


def main():
    compiler, library = map(lambda value: Path(value).resolve(), sys.argv[1:3])
    metadata = library.with_suffix('.nsmeta')
    assert library.is_file() and metadata.is_file()
    with tempfile.TemporaryDirectory(prefix='nsc-preserving-callee-') as temporary:
        root = Path(temporary)
        project = root / 'Consumer.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Consumer</Name>'
                           '</PropertyGroup><ItemGroup><Compile Include="Program.ns" />'
                           '<LibraryReference Include="' + library.as_posix() + '" Metadata="' +
                           metadata.as_posix() + '" /></ItemGroup></Project>', encoding='utf-8')
        source = root / 'Program.ns'
        for name, (accepted, body) in CASES.items():
            source.write_text(COMMON + HELPERS + body + CHECK, encoding='utf-8')
            result = run([compiler, '--check-project-method-bodies', project], root)
            output = result.stdout + result.stderr
            if not accepted:
                assert result.returncode != 0 and 'borrowed field view' in output, (name, output)
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
