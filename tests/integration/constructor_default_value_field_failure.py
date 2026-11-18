"""NSC-NEW-012: a value field with a destructor that the constructor never stores is dropped once.

A field such as `public Part field;` keeps its default value; the object's destructor drops that
value when construction succeeds, so it is live from the start of the constructor. Historical builds
dropped it on success but not when the constructor threw, so its destructor never ran. Each program
counts destructor calls and runs at O0 and O2:

- the failing constructor is the declaring one, a base reached from a derived constructor, or one
  that throws directly;
- a sibling field with an initializer is still dropped once (it becomes live at its store);
- a field of the derived type is not live yet when its base constructor fails;
- the derived constructor failing after a completed base drops the base and derived fields;
- success drops every field exactly once (no double drop on the normal path).
"""
from pathlib import Path
import subprocess
import sys
import tempfile

PRELUDE = '''namespace NativeSharp.Core { public class Exception { } }
using NativeSharp.Core;
struct Part { public static int Drops; ~Part() { Drops = Drops + 1; } }
class Gate {
 public static bool FailBase; public static bool FailDerived;
 public static int Base() throws Exception { if (FailBase) { throw new Exception(); } return 7; }
 public static int Derived() throws Exception { if (FailDerived) { throw new Exception(); } return 7; } }
class Base { public Part field; public Part other = new Part(); public int Number;
 public Base() throws Exception { Number = Gate.Base(); } }
class Derived : Base { public Part own; public Derived() throws Exception { int x = Gate.Derived(); } }
class Direct { public Part field; public Direct() throws Exception { throw new Exception(); } }
'''

CASES = {
    'success drops every field once': '''
 try { raii Derived d = new Derived(); } catch (Exception error) { return 2; }
 return Part.Drops == 3 ? 42 : 10 + Part.Drops;''',
    'declaring constructor fails': '''
 Gate.FailBase = true; int caught = 0;
 try { raii Base b = new Base(); } catch (Exception error) { caught = 1; }
 return caught == 1 && Part.Drops == 2 ? 42 : 10 + Part.Drops;''',
    'base fails under derived': '''
 Gate.FailBase = true; int caught = 0;
 try { raii Derived d = new Derived(); } catch (Exception error) { caught = 1; }
 return caught == 1 && Part.Drops == 2 ? 42 : 10 + Part.Drops;''',
    'derived fails after base': '''
 Gate.FailDerived = true; int caught = 0;
 try { raii Derived d = new Derived(); } catch (Exception error) { caught = 1; }
 return caught == 1 && Part.Drops == 3 ? 42 : 10 + Part.Drops;''',
    'constructor throws directly': '''
 int caught = 0;
 try { raii Direct d = new Direct(); } catch (Exception error) { caught = 1; }
 return caught == 1 && Part.Drops == 1 ? 42 : 10 + Part.Drops;''',
}

PROJECT = ('<Project><PropertyGroup><Name>DefaultField</Name><OutputType>Exe</OutputType>'
           '</PropertyGroup><ItemGroup><Compile Include="Program.ns" /></ItemGroup></Project>')


def run(args, cwd):
    return subprocess.run(args, cwd=cwd, capture_output=True, text=True, errors='replace', timeout=120)


def main():
    compiler = str(Path(sys.argv[1]).resolve())
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-default-field-failure-') as temporary:
        root = Path(temporary)
        for index, (name, body) in enumerate(CASES.items()):
            source = PRELUDE + 'class Program { public static int Main() {' + body + ' } }\n'
            for level in ('O0', 'O2'):
                folder = root / ('case-' + str(index) + '-' + level)
                folder.mkdir()
                (folder / 'Program.ns').write_text(source, encoding='utf-8')
                (folder / 'Case.nsproj').write_text(PROJECT, encoding='utf-8')
                build = run([compiler, '-' + level, '--emit-project-oop-exe', str(folder / 'Case.nsproj'),
                             '-o', str(folder / 'case.exe'), '--no-artifact-cache', '--no-error-log'], folder)
                if build.returncode:
                    failures.append((name, level, 'build', build.stdout + build.stderr))
                    continue
                executed = run([str(folder / 'case.exe')], folder)
                print(name, level, 'exit', executed.returncode, flush=True)
                if executed.returncode != 42:
                    failures.append((name, level, 'run', executed.returncode))
    if failures:
        print('FAIL', failures, flush=True)
        return 1
    print('constructor_default_value_field_failure: PASS', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
