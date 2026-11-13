"""Check value-field cleanup for structurally proven delegated constructors."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile

from isolated_a_runner import absolute_a, setup


SOURCE = '''using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception {} }
class Thing { public static int Drops; ~Thing() { Drops = Drops + 1; } }
struct Field { public static int Trace; public int Id; FIELD
 public Field(int id) { Id = id; INIT } ~Field() { Trace = Trace * 10 + Id; } }
KIND Bad { public static int Mode; public static int Drops;
 public Field field = new Field(1);
 public Bad(int marker) { }
 public Bad() throws Exception : this(0) {
  if (Mode == 1) throw new Exception();
  if (Mode == 2) { field = new Field(2); throw new Exception(); }
  if (Mode == 3) { Field local = move(field); throw new Exception(); }
 }
 ~Bad() { Drops = Drops + 1; } }
class Program { public static int Main() { Bad.Mode = MODE; int caught = 0;
 try { DECLARATION } catch (Exception error) { caught = caught + 1; }
 return caught == CAUGHT && Field.Trace == TRACE && Bad.Drops == DROPS &&
  Thing.Drops == THINGS ? 42 : 20; } }
'''

MOVED_SOURCE = '''using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception {} }
struct Field { public static int Drops; ~Field() { Drops = Drops + 1; } }
KIND Bad { public static int Drops; public Field field = new Field();
 public Bad(int marker) { Field local = move(field); }
 public Bad() throws Exception : this(1) { throw new Exception(); }
 ~Bad() { Drops = Drops + 1; } }
class Program { public static int Main() { int caught = 0;
 try { DECLARATION } catch (Exception error) { caught = 1; }
 return caught == 1 && Field.Drops == 1 && Bad.Drops == 0 ? 42 : 20; } }
'''

BASE_FAILURE = '''using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception {} }
struct Field { public static int Drops; ~Field() { Drops = Drops + 1; } }
class Base { public static int Calls; public Base() throws Exception { Calls = Calls + 1; throw new Exception(); } }
class Bad : Base { public static int Drops; public Field field = new Field();
 public Bad(int marker) throws Exception { } public Bad() throws Exception : this(1) { }
 ~Bad() { Drops = Drops + 1; } }
class Program { public static int Main() { int caught = 0;
 try { owned Bad value = new Bad(); } catch (Exception error) { caught = 1; }
 return caught == 1 && Base.Calls == 1 && Field.Drops == 0 && Bad.Drops == 0 ? 42 : 20; } }
'''


def run_case(compiler, root, name, source, repository, environment, failures):
    case = root / name
    case.mkdir()
    (case / 'Main.ns').write_text(source, encoding='utf-8')
    project = case / 'Probe.nsproj'
    project.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup>'
                       '<ItemGroup><Compile Include="Main.ns" /></ItemGroup></Project>', encoding='utf-8')
    for level in ('O0', 'O2'):
        exe = case / (level + '.exe')
        command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level,
                   '-o', str(exe), '--no-artifact-cache']
        print('COMMAND', subprocess.list2cmdline(command), 'CWD', repository, flush=True)
        result = subprocess.run(command, cwd=repository, env=environment, capture_output=True,
                                text=True, errors='replace', timeout=120)
        if result.returncode:
            failures.append((name, level, 'compile', result.stdout + result.stderr))
            continue
        result = subprocess.run([str(exe)], cwd=repository, env=environment, capture_output=True,
                                text=True, errors='replace', timeout=10)
        print('RUN', name, level, result.returncode, flush=True)
        if result.returncode != 42:
            failures.append((name, level, 'run', result.returncode))


def run(compiler, root):
    repository = Path(__file__).resolve().parents[2]
    environment = setup(root, repository)
    failures = []
    for kind, declaration in (('class', 'owned Bad value = new Bad();'),
                              ('struct', 'Bad value = new Bad();')):
        for owned in (False, True):
            for mode, caught, trace, drops, thing_count in ((1, 1, 1, 0, 1), (2, 1, 12, 0, 2),
                                                            (3, 1, 1, 0, 1), (4, 0, 1, 1, 1)):
                name = f'{kind}-{int(owned)}-{mode}'
                values = {'KIND': kind, 'MODE': str(mode), 'DECLARATION': declaration,
                          'CAUGHT': str(caught), 'TRACE': str(trace), 'DROPS': str(drops),
                          'THINGS': str(thing_count if owned else 0),
                          'FIELD': 'public owned Thing Item;' if owned else '',
                          'INIT': 'Item = new Thing();' if owned else ''}
                source = SOURCE
                for key, value in values.items():
                    source = source.replace(key, value)
                run_case(compiler, root, name, source, repository, environment, failures)
        moved = MOVED_SOURCE.replace('KIND', kind).replace('DECLARATION', declaration)
        run_case(compiler, root, kind + '-moved-before-return', moved, repository, environment, failures)
    run_case(compiler, root, 'base-failure', BASE_FAILURE, repository, environment, failures)
    for failure in failures:
        print('FAIL', *failure, file=sys.stderr)
    return int(bool(failures))


def main():
    if os.name != 'nt':
        return 77
    if len(sys.argv) not in (2, 3):
        raise SystemExit('expected nsc and optionally a new evidence directory on A:')
    compiler = Path(sys.argv[1]).resolve()
    temporary = Path(tempfile.gettempdir()).resolve()
    if compiler.anchor.lower() != 'a:\\' or temporary.anchor.lower() != 'a:\\':
        print('this focused regression requires the isolated compiler and temporary directory on A:')
        return 77
    if len(sys.argv) == 3:
        return run(absolute_a(sys.argv[1]), absolute_a(sys.argv[2]))
    with tempfile.TemporaryDirectory(prefix='nsc-delegated-field-', dir=temporary) as directory:
        return run(compiler, Path(directory) / 'cases')


if __name__ == '__main__':
    sys.exit(main())
