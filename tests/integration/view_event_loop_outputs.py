"""Check view output origins across loops through event invocations."""
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
prefix = fixture['TYPES'] + '''
public borrowed struct Carrier { public View field; }
delegate void SaveView(View input, ref View output);
delegate void SaveViewOut(View input, out View output);
'''
owner = fixture['OWNER']
cases = {
    'event_ref_escape': (False, owner + 'Signal += Sink.Save; View result = default(View); '
        '{ Owner inner; inner.Value = 7; View source = owner.Borrow(); int i = 0; '
        'while (i < 2) { Signal(source, ref result); source = inner.Borrow(); i = i + 1; } '
        '} return result.Get();'),
    'event_out_escape': (False, owner + 'SignalOut += Sink.SaveOut; View result = default(View); '
        '{ Owner inner; inner.Value = 7; View source = owner.Borrow(); int i = 0; '
        'while (i < 2) { SignalOut(source, out result); source = inner.Borrow(); i = i + 1; } '
        '} return result.Get();'),
    'event_ref_alias_inner_owner': (False, 'Signal += Sink.Save; View result = default(View); '
        '{ Owner inner; inner.Value = 7; View source = inner.Borrow(); ref View alias = ref result; '
        'Signal(source, ref alias); } return result.Get();'),
    'event_struct_field_inner_owner': (False, 'Signal += Sink.Save; Carrier holder = default(Carrier); '
        '{ Owner inner; inner.Value = 7; View source = inner.Borrow(); '
        'Signal(source, ref holder.field); } return holder.field.Get();'),
    'event_struct_field_owner_replaced': (False, owner + 'Signal += Sink.Save; '
        'Carrier holder = default(Carrier); { View source = owner.Borrow(); Signal(source, ref holder.field); } '
        'ref Owner alias = ref owner; alias = default(Owner); return holder.field.Get();'),
    'event_struct_field_loop_escape': (False, owner + 'Signal += Sink.Save; Carrier holder = default(Carrier); '
        '{ Owner inner; inner.Value = 7; View source = owner.Borrow(); int i = 0; '
        'while (i < 2) { Signal(source, ref holder.field); source = inner.Borrow(); i = i + 1; } '
        '} return holder.field.Get();'),
    'event_for_escape': (False, owner + 'Signal += Sink.Save; View result = default(View); '
        '{ Owner inner; inner.Value = 7; View source = owner.Borrow(); '
        'for (int i = 0; i < 2; i = i + 1) { Signal(source, ref result); source = inner.Borrow(); } '
        '} return result.Get();'),
    'event_delegate_chain_escape': (False, owner + 'Signal += Sink.Save; SaveView save = Sink.Save; '
        'View result = default(View); { Owner inner; inner.Value = 7; View first = owner.Borrow(); '
        'View second = first; int i = 0; while (i < 3) { Signal(second, ref result); '
        'save(first, ref second); first = inner.Borrow(); i = i + 1; } } return result.Get();'),
    'event_same_scope': (True, owner + 'Owner inner; inner.Value = 7; Signal += Sink.Save; '
        'View result = default(View); View source = owner.Borrow(); int i = 0; '
        'while (i < 2) { Signal(source, ref result); source = inner.Borrow(); i = i + 1; } '
        'return result.Get();'),
    'event_break_before_backedge': (True, owner + 'Signal += Sink.Save; View result = default(View); '
        '{ Owner inner; inner.Value = 7; View source = owner.Borrow(); '
        'while (true) { Signal(source, ref result); source = inner.Borrow(); break; } '
        '} return result.Get();'),
}
failures = []
with tempfile.TemporaryDirectory(prefix='nsc-view-event-loop-', dir=temporary) as directory:
    root = Path(directory)
    project = root / 'Probe.nsproj'
    project.write_text('<Project><PropertyGroup><Name>ViewEventLoop</Name></PropertyGroup>'
                       '<ItemGroup><Compile Include="Main.ns"/></ItemGroup></Project>', encoding='utf-8')
    for name, (valid, body) in cases.items():
        source = prefix + ('class Program { public static event SaveView Signal; '
            'public static event SaveViewOut SignalOut; public static int Main() { ' + body + ' } }')
        (root / 'Main.ns').write_text(source, encoding='utf-8')
        command = [str(compiler), '--check-project-method-bodies', str(project)]
        result = subprocess.run(command, cwd=repository, capture_output=True, text=True,
                                errors='replace', timeout=120)
        output = result.stdout + result.stderr
        expected = ('while a borrowed local is alive' if name == 'event_struct_field_owner_replaced'
                    else 'view output cannot outlive its possible owner')
        passed = result.returncode == 0 if valid else result.returncode != 0 and expected in output
        print(name, 'PASS' if passed else 'FAIL', result.returncode, flush=True)
        print('COMMAND', subprocess.list2cmdline(command), '\nSOURCE', source, '\nOUTPUT', output, flush=True)
        if not passed:
            failures.append(name)
print('FAILURES', failures, flush=True)
raise SystemExit(bool(failures))
