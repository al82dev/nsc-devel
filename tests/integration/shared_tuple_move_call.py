import os
from pathlib import Path
import subprocess
import sys
import tempfile

core_argument = sys.argv[2] if len(sys.argv) > 2 else os.environ.get('NSC_TEST_CORE_BINARY')
if not core_argument:
    print('SKIP: set NSC_TEST_CORE_BINARY to an isolated NativeSharp.Core library', flush=True)
    sys.exit(77)
compiler, core = (Path(value).resolve() for value in (sys.argv[1], core_argument))
failures = []
with tempfile.TemporaryDirectory(prefix='nsc-shared-tuple-call-') as directory:
    root = Path(directory)
    project = root / 'Probe.nsproj'
    project.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup><ItemGroup>'
        '<Compile Include="Main.ns"/><LibraryReference Include="' + core.as_posix() +
        '" Metadata="' + core.with_suffix('.nsmeta').as_posix() + '"/></ItemGroup></Project>', encoding='utf-8')
    for strong, weak in (('Rc', 'WeakRc'), ('Arc', 'WeakArc')):
        for kind, handle in (('strong', strong), ('weak', weak)):
            for throws in (False, True):
                action = 'throw new Exception();' if throws else ''
                declaration = ' throws Exception' if throws else ''
                before = (weak + '<Payload> observer = owner.Downgrade(); '
                          if kind == 'strong' else weak + '<Payload> value = owner.Downgrade(); ')
                operand = 'owner' if kind == 'strong' else 'value'
                verify = 'observer.Expired()' if kind == 'strong' else 'owner.WeakCount() == 0l'
                call = 'Consume((move(' + operand + '), 0));'
                if throws:
                    call = 'int caught = 0; try { ' + call + ' } catch (Exception e) { caught = 1; } '
                    verify = 'caught == 1 && ' + verify
                source = ('using NativeSharp.Core; class Payload { public Payload() { } } '
                    'class Program { static void Consume((' + handle + '<Payload>, int) pair)' +
                    declaration + ' { ' + action + ' } public static int Main() { ' + strong +
                    '<Payload> owner = SharedPointers.Make' + strong + '<Payload>(); ' + before + call +
                    ' return ' + verify + ' ? 42 : 1; } }')
                (root / 'Main.ns').write_text(source, encoding='utf-8')
                for level in ('O0', 'O2'):
                    exe = root / (handle + '-' + str(throws) + '-' + level + '.exe')
                    command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level,
                               '-o', str(exe), '--no-artifact-cache']
                    build = subprocess.run(command, cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
                    print('CASE', handle, 'throws', throws, level, 'COMMAND', subprocess.list2cmdline(command),
                          'EXIT', build.returncode, '\nSOURCE', source, '\nOUTPUT', build.stdout + build.stderr, flush=True)
                    if build.returncode:
                        failures.append((handle, throws, level, 'compile'))
                        continue
                    result = subprocess.run([str(exe)], cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
                    print('EXECUTE', exe.name, 'EXIT', result.returncode, result.stdout + result.stderr, flush=True)
                    if result.returncode != 42:
                        failures.append((handle, throws, level, 'run', result.returncode))
print('FAILURES', failures, flush=True)
sys.exit(bool(failures))
