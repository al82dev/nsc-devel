"""Check cleanup of shared-pointer fields on constructor failure with binary Core."""
from html import escape
from pathlib import Path
import subprocess
import sys
import tempfile


SOURCE = '''using NativeSharp.Core;
class Payload { public Payload() { } }
class Gate { static Gate() throws Exception { throw new Exception(); }
 public static int Read() { return 7; } }
class Holder { public HANDLE<Payload> value; public int number;
 public void Initialize(HANDLE<Payload> input) { value = move(input); HELPER_FAILURE }
 public Holder(HANDLE<Payload> input) { Initialize(move(input)); CONSTRUCTOR_FAILURE } }
class Program { public static int Main() {
 STRONG<Payload> owner = SharedPointers.MAKE<Payload>(); int caught = 0;
 try { Holder holder = new Holder(INPUT); } catch (Exception error) { caught = 1; }
 return caught == 1 && owner.COUNT() == EXPECTED ? 42 : 1;
} }
'''

TWO_FIELDS_SOURCE = '''using NativeSharp.Core;
class Payload { public Payload() { } }
class Gate { static Gate() throws Exception { throw new Exception(); }
 public static int Read() { return 7; } }
class Holder { public HANDLE<Payload> first; public HANDLE<Payload> second; public int number;
 public Holder(HANDLE<Payload> a, HANDLE<Payload> b) {
  first = move(a); second = move(b); number = Gate.Read(); } }
class Program { public static int Main() {
 STRONG<Payload> owner = SharedPointers.MAKE<Payload>(); int caught = 0;
 try { Holder holder = new Holder(INPUT, INPUT); } catch (Exception error) { caught = 1; }
 return caught == 1 && owner.COUNT() == EXPECTED ? 42 : 1;
} }
'''

MIXED_FIELDS_SOURCE = '''using NativeSharp.Core;
class Payload { public Payload() { } }
class Gate { static Gate() throws Exception { throw new Exception(); }
 public static int Read() { return 7; } }
class Holder { public Rc<Payload> local; public WeakRc<Payload> localWeak;
 public Arc<Payload> atomic; public WeakArc<Payload> atomicWeak;
 public Holder(Rc<Payload> a, WeakRc<Payload> b, Arc<Payload> c, WeakArc<Payload> d) {
  local = move(a); localWeak = move(b); atomic = move(c); atomicWeak = move(d);
  int ignored = Gate.Read(); } }
class Program { public static int Main() {
 Rc<Payload> left = SharedPointers.MakeRc<Payload>();
 Arc<Payload> right = SharedPointers.MakeArc<Payload>(); int caught = 0;
 try { Holder holder = new Holder(left.Clone(), left.Downgrade(), right.Clone(), right.Downgrade()); }
 catch (Exception error) { caught = 1; }
 return caught == 1 && left.StrongCount() == 1l && left.WeakCount() == 0l &&
  right.StrongCount() == 1l && right.WeakCount() == 0l ? 42 : 1;
} }
'''


def main():
    compiler, library, metadata = (Path(value).resolve() for value in sys.argv[1:4])
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-binary-shared-constructor-') as temporary:
        root = Path(temporary)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>BinarySharedConstructor</Name></PropertyGroup>'
            '<ItemGroup><Compile Include="Main.ns"/><LibraryReference Include="' +
            escape(library.as_posix(), quote=True) + '" Metadata="' +
            escape(metadata.as_posix(), quote=True) + '"/></ItemGroup></Project>', encoding='utf-8')
        for strong, weak, factory in (('Rc', 'WeakRc', 'MakeRc'), ('Arc', 'WeakArc', 'MakeArc')):
            for kind, handle, source, count, expected in (
                    ('strong', strong, 'owner.Clone()', 'StrongCount', '1l'),
                    ('weak', weak, 'owner.Downgrade()', 'WeakCount', '0l')):
                for where in ('constructor', 'helper', 'two-fields', 'second-field-fails'):
                    fixture = TWO_FIELDS_SOURCE if where in ('two-fields', 'second-field-fails') else SOURCE
                    content = fixture.replace('HANDLE', handle).replace('STRONG', strong).replace('MAKE', factory)
                    content = content.replace('INPUT', source).replace('COUNT', count).replace('EXPECTED', expected)
                    content = content.replace('HELPER_FAILURE', 'number = Gate.Read();' if where == 'helper' else '')
                    content = content.replace('CONSTRUCTOR_FAILURE', 'number = Gate.Read();' if where == 'constructor' else '')
                    if where == 'second-field-fails':
                        content = content.replace(' public Holder(',
                            ' public static ' + handle + '<Payload> Fail(' + handle + '<Payload> input) '
                            'throws Exception { int ignored = Gate.Read(); return move(input); }\n public Holder(')
                        content = content.replace('> a, ' + handle + '<Payload> b) {',
                                                  '> a, ' + handle + '<Payload> b) throws Exception {')
                        content = content.replace('first = move(a); second = move(b); number = Gate.Read();',
                                                  'first = move(a); second = Fail(move(b));')
                    (root / 'Main.ns').write_text(content, encoding='utf-8')
                    for level in ('O0', 'O2'):
                        name = strong + '-' + kind + '-' + where + '-' + level
                        executable = root / (name + '.exe')
                        command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level,
                                   '-o', str(executable), '--no-artifact-cache']
                        result = subprocess.run(command, cwd=root, capture_output=True, text=True,
                                                errors='replace', timeout=120)
                        print('COMMAND', subprocess.list2cmdline(command), 'EXIT', result.returncode,
                              result.stdout + result.stderr, flush=True)
                        if result.returncode:
                            failures.append((name, 'compile'))
                            continue
                        result = subprocess.run([str(executable)], cwd=root, capture_output=True, text=True,
                                                errors='replace', timeout=120)
                        print('EXECUTE', name, 'EXIT', result.returncode, result.stdout + result.stderr, flush=True)
                        if result.returncode != 42:
                            failures.append((name, 'execute', result.returncode))
        (root / 'Main.ns').write_text(MIXED_FIELDS_SOURCE, encoding='utf-8')
        for level in ('O0', 'O2'):
            executable = root / ('mixed-four-families-' + level + '.exe')
            command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level,
                       '-o', str(executable), '--no-artifact-cache']
            result = subprocess.run(command, cwd=root, capture_output=True, text=True,
                                    errors='replace', timeout=120)
            print('COMMAND', subprocess.list2cmdline(command), 'EXIT', result.returncode,
                  result.stdout + result.stderr, flush=True)
            if result.returncode:
                failures.append(('mixed-four-families', level, 'compile'))
                continue
            result = subprocess.run([str(executable)], cwd=root, capture_output=True,
                                    text=True, errors='replace', timeout=120)
            print('EXECUTE mixed-four-families', level, 'EXIT', result.returncode, flush=True)
            if result.returncode != 42:
                failures.append(('mixed-four-families', level, 'execute', result.returncode))
    print('FAILURES', failures, flush=True)
    return bool(failures)


if __name__ == '__main__':
    sys.exit(main())
