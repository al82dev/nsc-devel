import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def main():
    compiler = Path(sys.argv[1]).resolve()
    library = Path(sys.argv[2]).resolve() if len(sys.argv) > 2 else None
    sdk = Path(__file__).resolve().parents[2] / 'lib' / 'NativeSharp.Core'
    names = ('Send', 'Sync', 'SharedStorage', 'SharedControlStorage', 'SharedReferenceCounts',
             'SharedLocalControl', 'SharedAtomicControl', 'SharedBorrow',
             'SharedLocalHandle', 'SharedLocalWeak', 'SharedLocalFactory',
             'SharedAtomicHandle', 'SharedAtomicWeak', 'SharedAtomicFactory',
             'SharedView', 'Rc', 'WeakRc', 'Arc', 'WeakArc', 'SharedPointers')
    source = '''using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception { } }
class Payload { public static int Drops; public Payload() { } ~Payload() { Drops = Drops + 1; } }
class Program {
 public static async int Slow() { SUSPEND return 7; }
 public static async int Work(FAMILY<Payload> owner) throws Exception {
   try { TRYBODY } finally { DECLARATION }
   RETURN
 }
 public static async int Main() {
   FAMILY<Payload> original = SharedPointers.MakeFAMILY<Payload>(); int caught = 0;
   try { int value = await Work(original.Clone()); } catch (Exception error) { caught = 1; }
   return 10 + caught * 10 + (int)original.StrongCount();
 } }
'''
    cases = {
        'exception_suspended_clone': (True, True, True, 21),
        'exception_suspended_clone_explicit_ctor': (True, True, True, 21),
        'exception_suspended_no_clone': (True, True, False, 21),
        'exception_ready_no_clone': (True, False, False, 21),
        'exception_catch_suspended_clone': (True, True, True, 21),
        'exception_catch_nested_finally_clone': (True, True, True, 21),
        'exception_ready_clone': (True, False, True, 21),
        'normal_suspended_clone': (False, True, True, 11),
    }
    selected = os.environ.get('NSC_SHARED_FINALLY_CASE')
    if selected and selected not in cases:
        raise ValueError('unknown shared async finally case: ' + selected)
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-shared-async-finally-') as temporary:
        root = Path(temporary)
        if library is None:
            for name in names:
                shutil.copyfile(sdk / (name + '.ns'), root / (name + '.ns'))
            references = ''
        else:
            metadata = library.with_suffix('.nsmeta')
            assert library.is_file() and metadata.is_file()
            references = ('<LibraryReference Include="' + library.as_posix() +
                          '" Metadata="' + metadata.as_posix() + '" />')
            source = source.replace('namespace NativeSharp.Core { public class Exception { } }\n', '')
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name><Language>3</Language>'
                           '</PropertyGroup><ItemGroup><Compile Include="*.ns" />'
                           + references + '</ItemGroup></Project>', encoding='utf-8')
        for family in ('Rc', 'Arc'):
            for name, (throws, suspends, clone, expected) in cases.items():
                if selected and selected != name:
                    continue
                declaration = ('(FAMILY<Payload>, int) pair = (owner.Clone(), await Slow());'
                               if clone else '(int, int) pair = (1, await Slow());')
                text = source.replace('FAMILY', family).replace('SUSPEND', 'yield;' if suspends else '')
                if library is None and name == 'exception_suspended_clone_explicit_ctor':
                    text = text.replace('public class Exception { }',
                                        'public class Exception { public Exception() { } }')
                text = text.replace('TRYBODY', 'throw new Exception();' if throws else 'int marker = 0;')
                text = text.replace('DECLARATION', declaration.replace('FAMILY', family))
                if name.startswith('exception_catch_'):
                    action = declaration.replace('FAMILY', family)
                    if name == 'exception_catch_nested_finally_clone':
                        action = 'try { int marker = 0; } finally { ' + action + ' }'
                    text = text.replace('finally {', 'catch (Exception error) {', 1).replace(
                        declaration.replace('FAMILY', family) + ' }', action + ' throw; }', 1)
                text = text.replace('RETURN', '' if throws else 'return 0;')
                (root / 'Main.ns').write_text(text, encoding='utf-8')
                for level in ('O0', 'O2'):
                    exe = root / (family + '-' + name + '-' + level + '.exe')
                    command = [str(compiler), '--emit-project-oop-exe', str(project), '-o', str(exe),
                               '-' + level, '--no-artifact-cache']
                    build = subprocess.run(command, cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
                    if build.returncode:
                        failures.append((family, name, level, 'build', build.stdout + build.stderr))
                        continue
                    if os.name != 'nt':
                        print(f'{family} {name} {level}: compiled; execution skipped', flush=True)
                        continue
                    result = subprocess.run([str(exe)], cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
                    print(f'{family} {name} {level}: exit={result.returncode}, expected={expected}', flush=True)
                    if result.returncode != expected:
                        failures.append((family, name, level, 'run', result.returncode))
    for failure in failures:
        print(f'FAIL {failure}', file=sys.stderr)
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
