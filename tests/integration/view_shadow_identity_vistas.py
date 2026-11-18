"""Semantic-only controls for stable owner identity; unsafe programs are never run."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile

repo = Path(__file__).resolve().parents[2]
if len(sys.argv) != 2:
    raise SystemExit('usage: view_shadow_identity_vistas.py <nsc>')
nsc = Path(sys.argv[1]).resolve()
root = Path(tempfile.mkdtemp(prefix='view-shadow-identity-'))
(root / 'Probe.nsproj').write_text('<Project><PropertyGroup><Name>ViewShadowIdentity</Name></PropertyGroup>'
    '<ItemGroup><Compile Include="Main.ns"/></ItemGroup></Project>', encoding='utf-8')
prefix = '''public borrowed struct View<lifetime a> {
 private borrowed[a] int* target;
 public View(ref[a] int first, ref[a] int second) { unsafe("view") { target = &second; } }
 public ref[this] int Get() { unsafe("view") { return ref *target; } }
}
public struct Owner { public int Value; ~Owner() { } }
class Program { public static int Main() { '''
cases = {
    'outer-roots-unshadowed': (True, 'Owner first; first.Value = 1; Owner second; second.Value = 42; View source = new View(ref first.Value, ref second.Value); View result = default(View); { result = source; } return result.Get();'),
    'outer-first-shadowed': (True, 'Owner first; first.Value = 1; Owner second; second.Value = 42; View source = new View(ref first.Value, ref second.Value); View result = default(View); { Owner first; first.Value = 7; result = source; } return result.Get();'),
    'outer-second-shadowed': (True, 'Owner first; first.Value = 1; Owner second; second.Value = 42; View source = new View(ref first.Value, ref second.Value); View result = default(View); { Owner second; second.Value = 7; result = source; } return result.Get();'),
    'inner-first-escape': (False, 'Owner second; second.Value = 42; View result = default(View); { Owner first; first.Value = 1; View source = new View(ref first.Value, ref second.Value); { Owner first; first.Value = 7; result = source; } } return result.Get();'),
    'inner-second-escape': (False, 'Owner first; first.Value = 1; View result = default(View); { Owner second; second.Value = 42; View source = new View(ref first.Value, ref second.Value); { Owner second; second.Value = 7; result = source; } } return result.Get();'),
    'branch-shadowed-live': (True, 'Owner first; first.Value = 1; Owner second; second.Value = 42; View source = new View(ref first.Value, ref second.Value); View result = default(View); { Owner second; second.Value = 7; if (second.Value == 7) { result = source; } else { result = source; } } return result.Get();'),
    'branch-shadowed-escape': (False, 'Owner first; first.Value = 1; View result = default(View); { Owner second; second.Value = 42; View source = new View(ref first.Value, ref second.Value); { Owner second; second.Value = 7; if (second.Value == 7) { result = source; } else { result = source; } } } return result.Get();'),
}
records = []
for name, (expected_success, body) in cases.items():
    source = prefix + body + ' } }'
    (root / 'Main.ns').write_text(source, encoding='utf-8')
    command = [str(nsc), '--check-project-method-bodies', str(root / 'Probe.nsproj')]
    run = subprocess.run(command, cwd=repo, capture_output=True, text=True, errors='replace', timeout=120)
    records.append(dict(case=name, source=source, command=command, expected_success=expected_success,
                        exit=run.returncode, stdout=run.stdout, stderr=run.stderr))
    print(name, run.returncode, 'expected', 'success' if expected_success else 'rejection', flush=True)
    if expected_success and run.returncode == 0:
        records[-1]['executions'] = []
        for level in ('O0', 'O2'):
            output = root / (name + '-' + level + '.exe')
            emit = [str(nsc), '--emit-project-oop-exe', str(root / 'Probe.nsproj'), '-' + level,
                    '-o', str(output), '--no-artifact-cache']
            compiled = subprocess.run(emit, cwd=repo, capture_output=True, text=True, errors='replace', timeout=120)
            execution = subprocess.run([str(output)], cwd=repo, capture_output=True, text=True, errors='replace',
                                       timeout=120) if compiled.returncode == 0 else None
            records[-1]['executions'].append(dict(emit=emit, compile_exit=compiled.returncode,
                compile_stdout=compiled.stdout, compile_stderr=compiled.stderr,
                run_exit=execution.returncode if execution else None))
            print(name, level, 'compile', compiled.returncode, 'run', execution.returncode if execution else None,
                  flush=True)
(root / 'results.json').write_text(json.dumps(dict(nsc_sha256=hashlib.sha256(nsc.read_bytes()).hexdigest(),
    records=records), indent=2), encoding='utf-8')
print('RESULTS', root / 'results.json')
raise SystemExit(int(any((r['exit'] == 0) != r['expected_success'] or
    (not r['expected_success'] and 'cannot be stored in a longer-lived local' not in r['stderr']) or
    (r['expected_success'] and any(x['compile_exit'] != 0 or x['run_exit'] != 42
                                   for x in r.get('executions', []))) for r in records)))
