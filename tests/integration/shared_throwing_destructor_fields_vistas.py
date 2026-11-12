"""Observe owned field cleanup when a payload destructor raises implicitly."""
from pathlib import Path
import hashlib
import itertools
import json
import subprocess
import sys
import tempfile

if len(sys.argv) != 3:
    raise SystemExit('usage: shared_throwing_destructor_fields_vistas.py <nsc> <core.lib>')
repo = Path(__file__).resolve().parents[2]
compiler = Path(sys.argv[1]).resolve()
core = Path(sys.argv[2]).resolve()
metadata = core.with_suffix('.nsmeta')
root = Path(tempfile.mkdtemp(prefix='shared-throw-drop-fields-'))
project = root / 'Probe.nsproj'
project.write_text('<Project><PropertyGroup><Name>ThrowDropFields</Name></PropertyGroup>'
    '<ItemGroup><Compile Include="Main.ns"/><LibraryReference Include="' + core.as_posix() +
    '" Metadata="' + metadata.as_posix() + '"/></ItemGroup></Project>', encoding='utf-8')
source = '''using NativeSharp.Core;
class Resource { public static int Drops; public Resource() { } ~Resource() { Drops = Drops + 1; } }
class Gate { static Gate() throws Exception { throw new Exception(); } public static int Read() { return 1; } }
KIND Payload { public static int Drops; public owned Resource item;
 public Payload() { item = new Resource(); } ~Payload() { Drops = Drops + 1; int ignored = Gate.Read(); } }
class Program { public static int Main() { int caught = 0;
 try { DECLARATION } catch (Exception error) { caught = caught + 1; }
 if (caught != 1 || Payload.Drops != 1) return 3;
 return Resource.Drops == 1 ? 42 : Resource.Drops == 0 ? 20 : 4;
} }
'''
inputs = (compiler, core, metadata)
before = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
records = []
for family, kind in itertools.product(('direct', 'Rc', 'Arc'), ('class', 'struct')):
    declaration = ('owned Payload owner = new Payload();' if kind == 'class' else 'Payload owner = new Payload();') if family == 'direct' else (family + '<Payload> owner = SharedPointers.Make' + family + '<Payload>();')
    text = source.replace('KIND', kind).replace('DECLARATION', declaration)
    (root / 'Main.ns').write_text(text, encoding='utf-8')
    for opt in ('O0', 'O2'):
        exe = root / (family + '-' + kind + '-' + opt + '.exe')
        command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + opt,
                   '-o', str(exe), '--no-artifact-cache']
        built = subprocess.run(command, cwd=repo, capture_output=True, text=True, errors='replace', timeout=120)
        executed = subprocess.run([str(exe)], cwd=repo, capture_output=True, text=True, errors='replace',
                                  timeout=30) if built.returncode == 0 else None
        records.append(dict(family=family, kind=kind, opt=opt, source=text, command=command,
                            compile_exit=built.returncode, stdout=built.stdout, stderr=built.stderr,
                            run_exit=executed.returncode if executed else None))
        print(family, kind, opt, 'compile', built.returncode, 'run', executed.returncode if executed else None, flush=True)
after = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
(root / 'results.json').write_text(json.dumps(dict(before=before, after=after, records=records), indent=2),
                                   encoding='utf-8')
print('RESULTS', root / 'results.json')
raise SystemExit(int(before != after or any(r['compile_exit'] != 0 or r['run_exit'] != 42 for r in records)))
