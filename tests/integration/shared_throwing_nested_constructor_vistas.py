"""Check nested constructor failure through the public throwing factories."""
from pathlib import Path
import hashlib
import json
import itertools
import subprocess
import sys
import tempfile

if len(sys.argv) != 3:
    raise SystemExit('usage: shared_throwing_nested_constructor_vistas.py <nsc> <core.lib>')
repo = Path(__file__).resolve().parents[2]
compiler = Path(sys.argv[1]).resolve()
core = Path(sys.argv[2]).resolve()
metadata = core.with_suffix('.nsmeta')
root = Path(tempfile.mkdtemp(prefix='shared-nested-constructor-'))
project = root / 'Probe.nsproj'
project.write_text('<Project><PropertyGroup><Name>NestedConstructorFailure</Name></PropertyGroup>'
    '<ItemGroup><Compile Include="Main.ns"/><LibraryReference Include="' + core.as_posix() +
    '" Metadata="' + metadata.as_posix() + '"/></ItemGroup></Project>', encoding='utf-8')
source = '''using NativeSharp.Core;
class Item { public static int Trace; public int Id; public Item(int id) { Id = id; }
 ~Item() { Trace = Trace * 10 + Id; } }
class Part { public static int Drops; public owned Item child;
 public Part() throws Exception { child = new Item(2); throw new Exception(); }
 ~Part() { Drops = Drops + 1; } }
KIND Payload { public static int Drops; public owned Item first; public owned Part second;
 public Payload() throws Exception { first = new Item(1); second = new Part(); }
 ~Payload() { Drops = Drops + 1; } }
class Program { public static int Main() { int caught = 0;
 try { STRONG<Payload> owner = SharedPointers.MAKE<Payload>(); }
 catch (Exception error) { caught = caught + 1; }
 return caught == 1 && Item.Trace == 21 && Part.Drops == 0 && Payload.Drops == 0 ? 42 : 1;
} }
'''
inputs = [compiler, core, metadata]
before = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
records = []
for strong, kind in itertools.product(('Rc', 'Arc'), ('class', 'struct')):
    text = source.replace('STRONG', strong).replace('KIND', kind).replace('MAKE', 'Make' + strong + 'Throwing')
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
