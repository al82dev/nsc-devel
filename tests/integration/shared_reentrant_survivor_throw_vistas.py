"""Keep a weak cloned in a last-owner destructor that exits by exception."""
from pathlib import Path
import hashlib
import json
import itertools
import subprocess
import sys
import tempfile

if len(sys.argv) != 3:
    raise SystemExit('usage: shared_reentrant_survivor_throw_vistas.py <nsc> <core.lib>')
repo = Path(__file__).resolve().parents[2]
compiler = Path(sys.argv[1]).resolve()
core = Path(sys.argv[2]).resolve()
metadata = core.with_suffix('.nsmeta')
root = Path(tempfile.mkdtemp(prefix='shared-weak-survivor-throw-'))
project = root / 'Probe.nsproj'
project.write_text('<Project><PropertyGroup><Name>WeakSurvivorThrow</Name></PropertyGroup>'
    '<ItemGroup><Compile Include="Main.ns"/><LibraryReference Include="' + core.as_posix() +
    '" Metadata="' + metadata.as_posix() + '"/></ItemGroup></Project>', encoding='utf-8')
source = '''using NativeSharp.Core;
class Observer { public static nint Address; public static nint Sink; public static int Drops; public static int Result; }
class Gate { static Gate() throws Exception { throw new Exception(); } public static int Read() { return 1; } }
KIND Payload { public Payload() { } ~Payload() {
 Observer.Drops = Observer.Drops + 1;
 unsafe("observe a live weak in the same thread") {
  WEAK<Payload>* weak = (WEAK<Payload>*)Observer.Address;
  WEAK<Payload>* sink = (WEAK<Payload>*)Observer.Sink;
  if ((*weak).WeakCount() != 1l) { Observer.Result = 2; return; }
  *sink = (*weak).Clone();
  Observer.Result = (*sink).Expired() && (*sink).WeakCount() == 2l && (*weak).WeakCount() == 2l ? 42 : 3;
 }
 int ignored = Gate.Read();
} }
class Program { public static int Main() {
 WEAK<Payload> weak; WEAK<Payload> survivor; int caught = 0;
 unsafe("observe live weak values in the same thread") { Observer.Address = (nint)&weak; Observer.Sink = (nint)&survivor; }
 try { STRONG<Payload> owner = SharedPointers.MAKE<Payload>(); weak = owner.Downgrade(); }
 catch (Exception error) { caught = caught + 1; }
 if (caught != 1 || Observer.Drops != 1 || Observer.Result != 42 || !weak.Expired() || weak.WeakCount() != 2l) return 1;
 weak = default(WEAK<Payload>);
 STRONG<Payload> resurrected = survivor.Upgrade();
 if (!survivor.Expired() || survivor.WeakCount() != 1l || !resurrected.IsEmpty()) return 2;
 return 42;
} }
'''
inputs = [compiler, core, metadata]
before = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
records = []
for strong, kind in itertools.product(('Rc', 'Arc'), ('class', 'struct')):
    text = source.replace('STRONG', strong).replace('WEAK', 'Weak' + strong).replace('KIND', kind).replace('MAKE', 'Make' + strong)
    (root / 'Main.ns').write_text(text, encoding='utf-8')
    for opt in ('O0', 'O2'):
        exe = root / (strong + '-' + kind + '-' + opt + '.exe')
        command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + opt,
                   '-o', str(exe), '--no-artifact-cache']
        built = subprocess.run(command, cwd=repo, capture_output=True, text=True, errors='replace', timeout=120)
        executed = subprocess.run([str(exe)], cwd=repo, capture_output=True, text=True, errors='replace',
                                  timeout=30) if built.returncode == 0 else None
        records.append(dict(family=strong, kind=kind, opt=opt, source=text, command=command,
                            compile_exit=built.returncode, stdout=built.stdout, stderr=built.stderr,
                            run_exit=executed.returncode if executed else None))
        print(strong, kind, opt, 'compile', built.returncode, 'run', executed.returncode if executed else None, flush=True)
after = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
(root / 'results.json').write_text(json.dumps(dict(before=before, after=after, records=records), indent=2),
                                   encoding='utf-8')
print('RESULTS', root / 'results.json')
raise SystemExit(int(before != after or any(r['compile_exit'] != 0 or r['run_exit'] != 42 for r in records)))
