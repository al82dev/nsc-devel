"""Do not let a view of a local escape through an alias of an output parameter."""
import os
from pathlib import Path
import runpy
import subprocess
import sys
import tempfile

if len(sys.argv) != 2 or os.name != 'nt':
    raise SystemExit('expected Windows nsc on A:')
compiler = Path(sys.argv[1]).resolve()
temporary = Path(os.environ['TEMP']).resolve()
if compiler.drive.upper() != 'A:' or temporary.drive.upper() != 'A:':
    raise ValueError('compiler and temporary directory must be on A:')
repository = Path(__file__).resolve().parents[2]
fixture = runpy.run_path(str(Path(__file__).with_name('view_ref_owner_replacement.py')))
cases = {
    'ref_alias_escape': (False, 'Owner inner; inner.Value = 7; View source = inner.Borrow(); '
        'ref View alias = ref output; Sink.Save(source, ref alias);'),
    'out_alias_escape': (False, 'Owner inner; inner.Value = 7; View source = inner.Borrow(); '
        'ref View alias = ref output; Sink.SaveOut(source, out alias);'),
    'reborrow_alias_escape': (False, 'Owner inner; inner.Value = 7; View source = inner.Borrow(); '
        'ref View alias = ref output; ref View second = ref alias; Sink.Save(source, ref second);'),
    'out_parameter_alias_escape': (False, 'output = default(View); Owner inner; inner.Value = 7; '
        'View source = inner.Borrow(); ref View alias = ref output; Sink.Save(source, ref alias);'),
    'same_scope_control': (True, 'Sink.Save(input, ref output);'),
    'parameter_alias_control': (True, 'ref View alias = ref output; Sink.Save(input, ref alias);'),
    'out_parameter_alias_control': (True, 'output = default(View); ref View alias = ref output; '
        'Sink.Save(input, ref alias);'),
}
failures = []
with tempfile.TemporaryDirectory(prefix='nsc-view-parameter-alias-', dir=temporary) as directory:
    root = Path(directory)
    project = root / 'Probe.nsproj'
    project.write_text('<Project><PropertyGroup><Name>ViewParameterAlias</Name></PropertyGroup>'
                       '<ItemGroup><Compile Include="Main.ns"/></ItemGroup></Project>', encoding='utf-8')
    for name, (valid, body) in cases.items():
        mode = 'out' if name.startswith('out_parameter_') else 'ref'
        source = fixture['TYPES'] + ('class Forwarder { public static void Run(View input, ' + mode + ' View output) { '
            + body + ' } } class Program { public static int Main() { Owner owner; owner.Value = 42; '
            'View input = owner.Borrow(); View result = default(View); Forwarder.Run(input, ' + mode + ' result); '
            'return result.Get(); } }')
        (root / 'Main.ns').write_text(source, encoding='utf-8')
        command = [str(compiler), '--check-project-method-bodies', str(project)]
        result = subprocess.run(command, cwd=repository, capture_output=True, text=True,
                                errors='replace', timeout=120)
        output = result.stdout + result.stderr
        passed = result.returncode == 0 if valid else (
            result.returncode != 0 and 'view output cannot outlive its possible owner' in output)
        print(name, 'PASS' if passed else 'FAIL', result.returncode, flush=True)
        print('COMMAND', subprocess.list2cmdline(command), '\nSOURCE', source, '\nOUTPUT', output, flush=True)
        if not passed:
            failures.append(name)
print('FAILURES', failures, flush=True)
raise SystemExit(bool(failures))
