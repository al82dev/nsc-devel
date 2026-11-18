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
forms = {
    'direct_move': ('HANDLE<Payload>', 'return move(input);'),
    'tuple_move': ('(int, HANDLE<Payload>)', 'return (42, move(input));'),
    'local_tuple_move': ('(int, HANDLE<Payload>)',
        '(int, HANDLE<Payload>) pair = (42, move(input)); return move(pair);'),
    'tuple_clone': ('(int, HANDLE<Payload>)', 'return (42, input.Clone());'),
    'use_after_move': ('(int, HANDLE<Payload>)',
        '(int, HANDLE<Payload>) pair = (42, move(input)); input.Clone(); return move(pair);'),
    'double_move': ('(HANDLE<Payload>, HANDLE<Payload>)', 'return (move(input), move(input));'),
    'implicit_copy': ('(int, HANDLE<Payload>)', 'return (42, input);'),
    'discard_move': ('HANDLE<Payload>', '(move(input), 0); return input.Clone();'),
}
failures = []
with tempfile.TemporaryDirectory(prefix='nsc-shared-tuple-move-') as directory:
    root = Path(directory)
    project = root / 'Probe.nsproj'
    project.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup><ItemGroup>'
        '<Compile Include="Main.ns"/><LibraryReference Include="' + core.as_posix() +
        '" Metadata="' + core.with_suffix('.nsmeta').as_posix() + '"/></ItemGroup></Project>', encoding='utf-8')
    for handle in ('Rc', 'WeakRc', 'Arc', 'WeakArc'):
        for form, (return_type, body) in forms.items():
            source = ('using NativeSharp.Core; public class Payload { public Payload() { } } '
                'public class Box { public static ' + return_type + ' Pack(HANDLE<Payload> input) { ' +
                body + ' } }').replace('HANDLE', handle)
            (root / 'Main.ns').write_text(source, encoding='utf-8')
            command = [str(compiler), '--check-project-method-bodies', str(project)]
            result = subprocess.run(command, cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
            print('CASE', handle, form, 'COMMAND', subprocess.list2cmdline(command), 'EXIT', result.returncode,
                  '\nSOURCE', source, '\nOUTPUT', result.stdout + result.stderr, flush=True)
            should_reject = form in ('use_after_move', 'double_move', 'implicit_copy', 'discard_move')
            if (result.returncode != 0) != should_reject:
                failures.append((handle, form, 'unexpected result'))
            required = {'use_after_move': 'use of moved-from parameter',
                        'double_move': 'cannot move from already moved-from parameter',
                        'implicit_copy': 'move-only value cannot be copied into a tuple element',
                        'discard_move': 'in a consuming context'}.get(form)
            if required and required not in result.stdout + result.stderr:
                failures.append((handle, form, 'missing ownership diagnostic'))
print('FAILURES', failures, flush=True)
sys.exit(bool(failures))
