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
with tempfile.TemporaryDirectory(prefix='nsc-shared-tuple-discard-') as directory:
    root = Path(directory)
    project = root / 'Probe.nsproj'
    project.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup><ItemGroup>'
        '<Compile Include="Main.ns"/><LibraryReference Include="' + core.as_posix() +
        '" Metadata="' + core.with_suffix('.nsmeta').as_posix() + '"/></ItemGroup></Project>', encoding='utf-8')
    for strong, weak in (('Rc', 'WeakRc'), ('Arc', 'WeakArc')):
        for handle, operation, count, expected in ((strong, 'Clone', 'StrongCount', '1l'),
                                                    (weak, 'Downgrade', 'WeakCount', '0l')):
            for variant in ('discard', 'partial', 'local'):
                expression = '(owner.' + operation + '(), ' + ('Fail()' if variant == 'partial' else '0') + ')'
                statement = expression + ';'
                verify = 'owner.' + count + '() == ' + expected
                if variant == 'partial':
                    statement = 'int caught = 0; try { ' + statement + ' } catch (Exception e) { caught = 1; }'
                    verify = 'caught == 1 && ' + verify
                elif variant == 'local':
                    statement = '(' + handle + '<Payload>, int) pair = ' + expression + ';'
                    verify = 'owner.' + count + '() == ' + ('2l' if handle == strong else '1l')
                source = ('using NativeSharp.Core; class Payload { public Payload() { } } class Program { '
                    'static int Fail() throws Exception { throw new Exception(); } public static int Main() { '
                    + strong + '<Payload> owner = SharedPointers.Make' + strong + '<Payload>(); (1, 2); ' + statement +
                    ' return ' + verify + ' ? 42 : 2; } }')
                (root / 'Main.ns').write_text(source, encoding='utf-8')
                for level in ('O0', 'O2'):
                    executable = root / (handle + '-' + variant + '-' + level + '.exe')
                    command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level,
                               '-o', str(executable), '--no-artifact-cache']
                    build = subprocess.run(command, cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
                    print('CASE', handle, variant, level, '\nSOURCE', source, '\nCOMMAND',
                          subprocess.list2cmdline(command), 'EXIT', build.returncode,
                          build.stdout + build.stderr, flush=True)
                    if build.returncode:
                        failures.append((handle, variant, level, 'compile'))
                        continue
                    result = subprocess.run([str(executable)], cwd=root, capture_output=True,
                                            text=True, errors='replace', timeout=120)
                    print('EXECUTE', executable.name, 'EXIT', result.returncode,
                          result.stdout + result.stderr, flush=True)
                    if result.returncode != 42:
                        failures.append((handle, variant, level, 'execute', result.returncode))
print('FAILURES', failures, flush=True)
sys.exit(bool(failures))
