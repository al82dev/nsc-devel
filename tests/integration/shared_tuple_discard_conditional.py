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
with tempfile.TemporaryDirectory(prefix='nsc-tuple-discard-conditional-') as directory:
    root = Path(directory)
    project = root / 'Probe.nsproj'
    project.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup><ItemGroup>'
        '<Compile Include="Main.ns"/><LibraryReference Include="' + core.as_posix() +
        '" Metadata="' + core.with_suffix('.nsmeta').as_posix() + '"/></ItemGroup></Project>', encoding='utf-8')
    for strong, weak in (('Rc', 'WeakRc'), ('Arc', 'WeakArc')):
        for handle, operation, count, baseline in ((strong, 'Clone', 'StrongCount', 1),
                                                   (weak, 'Downgrade', 'WeakCount', 0)):
            for choice in ('true', 'false'):
                for variant in ('discard', 'partial', 'local'):
                    suffix = 'Fail()' if variant == 'partial' else '0'
                    expression = '(choose ? (first.' + operation + '(), ' + suffix + ') : ' + \
                                 '(second.' + operation + '(), ' + suffix + '))'
                    statement = expression + ';'
                    extra = ''
                    if variant == 'partial':
                        statement = 'int caught = 0; try { ' + statement + ' } catch (Exception e) { caught = 1; }'
                        extra = 'caught == 1 && '
                    elif variant == 'local':
                        statement = '(' + handle + '<Payload>, int) pair = ' + expression + ';'
                    first_count = baseline + (variant == 'local' and choice == 'true')
                    second_count = baseline + (variant == 'local' and choice == 'false')
                    source = ('using NativeSharp.Core; class Payload { public Payload() { } } class Program { '
                        'static int Fail() throws Exception { throw new Exception(); } public static int Main() { '
                        + strong + '<Payload> first = SharedPointers.Make' + strong + '<Payload>(); ' + strong +
                        '<Payload> second = SharedPointers.Make' + strong + '<Payload>(); bool choose = ' + choice +
                        '; ' + statement + ' return ' + extra + 'first.' + count + '() == ' + str(first_count) +
                        'l && second.' + count + '() == ' + str(second_count) + 'l ? 42 : 2; } }')
                    (root / 'Main.ns').write_text(source, encoding='utf-8')
                    for level in ('O0', 'O2'):
                        executable = root / (handle + '-' + choice + '-' + variant + '-' + level + '.exe')
                        command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level,
                                   '-o', str(executable), '--no-artifact-cache']
                        build = subprocess.run(command, cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
                        print('CASE', handle, choice, variant, level, '\nSOURCE', source, '\nCOMMAND',
                              subprocess.list2cmdline(command), 'EXIT', build.returncode,
                              build.stdout + build.stderr, flush=True)
                        if build.returncode:
                            failures.append((handle, choice, variant, level, 'compile'))
                            continue
                        result = subprocess.run([str(executable)], cwd=root, capture_output=True,
                                                text=True, errors='replace', timeout=120)
                        print('EXECUTE', executable.name, 'EXIT', result.returncode,
                              result.stdout + result.stderr, flush=True)
                        if result.returncode != 42:
                            failures.append((handle, choice, variant, level, 'execute', result.returncode))
    source = ('using NativeSharp.Core; class Payload { public Payload() { } } class Program { '
        'public static int Main() { Rc<Payload> owner = SharedPointers.MakeRc<Payload>(); '
        '(Rc<Payload>, int) pair = (owner.Clone(), 0); '
        '(true ? pair : (owner.Clone(), 0)); return 42; } }')
    (root / 'Main.ns').write_text(source, encoding='utf-8')
    command = [str(compiler), '--check-project-method-bodies', str(project)]
    check = subprocess.run(command, cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
    print('CASE move-only-copy-rejected', '\nSOURCE', source, '\nCOMMAND',
          subprocess.list2cmdline(command), 'EXIT', check.returncode, check.stdout + check.stderr, flush=True)
    if check.returncode == 0 or 'move-only value cannot be copied by a conditional expression' not in check.stdout + check.stderr:
        failures.append(('move-only-copy-rejected', 'analysis'))
print('FAILURES', failures, flush=True)
sys.exit(bool(failures))
