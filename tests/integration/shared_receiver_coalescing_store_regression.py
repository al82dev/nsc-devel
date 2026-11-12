"""Check receiver publication through coalescing stores; never execute consumers."""
import hashlib
import os
from pathlib import Path
import subprocess
import sys
import tempfile
from xml.sax.saxutils import escape

compiler = Path(sys.argv[1]).resolve()
core_name = os.environ.get('NSC_TEST_CORE_BINARY')
if not core_name:
    print('SKIP: set NSC_TEST_CORE_BINARY to an isolated Core library', flush=True)
    sys.exit(77)
core = Path(core_name).resolve()
metadata = core.with_suffix('.nsmeta')
if not core.is_file() or not metadata.is_file():
    raise SystemExit('configured Core library or metadata is missing')
for key in ('TEMP', 'TMP', 'APPDATA', 'LOCALAPPDATA'):
    value = os.environ.get(key)
    if not value or not Path(value).is_dir() or Path(value).resolve().drive.upper() != 'A:':
        print('SKIP: ' + key + ' must resolve to an existing directory on A:', flush=True)
        sys.exit(77)
temp_root = Path(os.environ.get('NSC_TEST_TEMP_ROOT') or os.environ['TEMP']).resolve()
if temp_root.drive.upper() != 'A:' or not temp_root.is_dir():
    print('SKIP: temporary root must be an existing directory on A:', flush=True)
    sys.exit(77)
repo = Path(__file__).resolve().parents[2]
attr = lambda path: escape(path.as_posix(), {'"': '&quot;'})
def hashes():
    return {str(path): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in (compiler, core, metadata)}

before = hashes()
print('SHA256 BEFORE', before, flush=True)
failures = []
with tempfile.TemporaryDirectory(prefix='nsc-shared-coalescing-store-', dir=temp_root) as temporary:
    root = Path(temporary)
    env = os.environ.copy()
    for key in ('TEMP', 'TMP', 'APPDATA', 'LOCALAPPDATA'):
        env[key] = str(root)
    def invoke(command):
        command = [str(value) for value in command]
        result = subprocess.run(command, cwd=repo, env=env, capture_output=True,
                                text=True, errors='replace', timeout=120)
        print('CWD', repo, 'COMMAND', subprocess.list2cmdline(command), 'EXIT', result.returncode,
              result.stdout + result.stderr, flush=True)
        return result
    for family in ('Rc', 'Arc'):
        for dispatch in ('direct', 'store', 'field', 'local'):
            for phase in ('constructor', 'destructor'):
                for publish in (True, False):
                    label = '-'.join((family, dispatch, phase, 'publishes' if publish else 'safe'))
                    folder = root / label
                    folder.mkdir()
                    call = {
                        'direct': 'Alias = this;' if publish else 'Alias = null;',
                        'store': 'Alias ??= this;' if publish else 'Alias ??= Other;',
                        'field': 'Box.Item ??= this;' if publish else 'Box.Item ??= Other;',
                        'local': 'Payload local = null; local ??= this; Alias = local;' if publish else 'Payload local = null; local ??= this;',
                    }[dispatch]
                    ctor = call if phase == 'constructor' else ''
                    dtor = call if phase == 'destructor' else ''
                    source = ('using NativeSharp.Core; class Holder { public Payload Item; } class Payload' +
                        ' { public static Payload Alias; public static Payload Other; public static Holder Box; public Payload() { ' + ctor + ' } ~Payload() { ' +
                        dtor + ' } } class Program { public static int Main() { ' + family + '<Payload> owner = '
                        'SharedPointers.Make' + family + '<Payload>(); return 0; } }')
                    (folder / 'Main.ns').write_text(source, encoding='utf-8')
                    project = folder / 'Probe.nsproj'
                    project.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup><ItemGroup>'
                        '<Compile Include="Main.ns"/><LibraryReference Include="' + attr(core) +
                        '" Metadata="' + attr(metadata) +
                        '"/></ItemGroup></Project>', encoding='utf-8')
                    print('CASE', label, flush=True)
                    if invoke([compiler, '--check-project-method-bodies', project]).returncode:
                        failures.append((label, 'semantic'))
                        continue
                    executable = folder / 'probe.exe'
                    result = invoke([compiler, '--emit-project-oop-exe', project, '-O0', '-o',
                                     executable, '--no-artifact-cache'])
                    output = result.stdout + result.stderr
                    if publish:
                        if result.returncode == 0 or 'shared payload publishes its receiver during construction or destruction' not in output:
                            failures.append((label, 'wrong-rejection'))
                    elif result.returncode != 0 or not executable.is_file():
                        failures.append((label, 'unexpected-rejection'))
after = hashes()
print('SHA256 AFTER', after, flush=True)
if before != after:
    failures.append(('input-hash-changed',))
if failures:
    print('FAIL', failures, file=sys.stderr, flush=True)
    sys.exit(1)
print('PASS: 32 emission checks; consumers were not executed', flush=True)
