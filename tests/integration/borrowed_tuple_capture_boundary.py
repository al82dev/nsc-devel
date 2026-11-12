"""Check captures of view-containing tuples without executing unsafe programs.

Arguments: compiler and optional separately built NativeSharpCore.lib. If the
second argument is omitted, NSC_TEST_CORE_BINARY selects the library. The
caller must set TEMP/TMP to an isolated directory and record both inputs.
"""
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
repo = Path(__file__).resolve().parents[2]
temp_name = os.environ.get('NSC_TEST_TEMP_ROOT') or os.environ.get('TEMP')
if not core.is_file() or not core.with_suffix('.nsmeta').is_file():
    print('ERROR: configured Core library and metadata are required', flush=True)
    sys.exit(1)
if not temp_name or Path(temp_name).drive.upper() != 'A:' or not Path(temp_name).is_dir():
    print('SKIP: NSC_TEST_TEMP_ROOT or TEMP must name an existing directory on A:', flush=True)
    sys.exit(77)
prefix = '''using NativeSharp.Core;
class Payload { public int Data; public Payload() { Data = 42; } }
class Program { public delegate void Action();
'''
local = '''public static int Main() { Action work;
 { FAMILY<Payload> owner = SharedPointers.FACTORY<Payload>();
 (SharedView<Payload>, int) saved = (owner.Borrow(), 0);
 work = [MODE saved] () => { var (view, n) = saved; int x = view.Get().Data + n; }; }
 work(); return 0; }'''
parameter = '''public static Action Capture((SharedView<Payload>, int) saved) {
 return [copy saved] () => { var (view, n) = saved; int x = view.Get().Data + n; }; }
 public static int Main() { return 0; }'''
ordinary = '''public static int Main() { (int, int) saved = (40, 2);
 Action work = [copy saved] () => { var (a, b) = saved; int x = a + b; };
 work(); return 0; }'''

with tempfile.TemporaryDirectory(prefix='borrowed-tuple-capture-', dir=temp_name) as directory:
    root = Path(directory)
    env = os.environ.copy()
    for key in ('TEMP', 'TMP', 'APPDATA', 'LOCALAPPDATA'):
        env[key] = str(root)
    project = root / 'Probe.nsproj'
    source = root / 'Main.ns'
    project.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup><ItemGroup>'
        '<Compile Include="Main.ns"/><LibraryReference Include="' + core.as_posix() +
        '" Metadata="' + core.with_suffix('.nsmeta').as_posix() +
        '"/></ItemGroup></Project>', encoding='utf-8')

    def check(name, body, marker=None):
        source.write_text(prefix + body + '\n}', encoding='utf-8')
        command = [str(compiler), '--check-project-method-bodies', str(project)]
        result = subprocess.run(command, cwd=repo, env=env, capture_output=True, text=True,
                                errors='replace', timeout=120)
        output = result.stdout + result.stderr
        print('CASE', name, 'COMMAND', subprocess.list2cmdline(command),
              'EXIT', result.returncode, output, flush=True)
        if marker is None:
            if result.returncode != 0:
                raise SystemExit('valid control rejected: ' + name)
        elif result.returncode == 0 or marker not in output:
            raise SystemExit('expected capture diagnostic missing: ' + name)

    for family in ('Rc', 'Arc'):
        for mode in ('copy', 'ref'):
            body = local.replace('FAMILY', family).replace('FACTORY', 'Make' + family).replace('MODE', mode)
            check(family + '-' + mode, body, "view local 'saved'")
    check('parameter', parameter, "view parameter 'saved'")
    check('ordinary-tuple', ordinary)
    print('PASS: unsafe captures analyzed only; ordinary tuple accepted', flush=True)
