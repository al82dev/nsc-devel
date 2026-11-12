"""Generate isolated lifecycle fixtures; compilation/execution require explicit flags."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys

REPOSITORY = Path(__file__).resolve().parents[2]

TEMPLATE = '''using NativeSharp.Core;
class Payload { public static int Drops; public static int Trace; public static int Next; public int Id;
 public Payload() { Next = Next + 1; Id = Next; }
 ~Payload() { Drops = Drops + 1; Trace = Trace * 10 + Id; } }
class Flags { public static int Mode;
 public static int Argument() throws Exception { if (Mode == 1) throw new Exception(); return 1; } }
class Base { public static int BaseDrops; public Rc<Payload> baseStrong; public WeakRc<Payload> baseWeak;
 public Base() throws Exception { baseStrong = SharedPointers.MakeRc<Payload>(); baseWeak = baseStrong.Downgrade();
  if (Flags.Mode == 2) throw new Exception(); }
 ~Base() { BaseDrops = BaseDrops + 1; } }
class Failed : Base { public static int FailedDrops; public Arc<Payload> strong; public WeakArc<Payload> weak;
 public Failed() throws Exception : this(Flags.Argument()) { if (Flags.Mode == 4) throw new Exception(); }
 public Failed(int marker) throws Exception : this(marker, 0) { }
 public Failed(int marker, int ignored) throws Exception {
  strong = SharedPointers.MakeArc<Payload>(); weak = strong.Downgrade();
  if (Flags.Mode == 3) throw new Exception(); }
 ~Failed() { FailedDrops = FailedDrops + 1; } }
class Program { public static int Main() { Flags.Mode = MODE; int caught = 0;
 try { CONSTRUCT } catch (Exception error) { caught = caught + 1; }
 return caught == CATCHES && Payload.Drops == PAYLOADS && Payload.Trace == TRACE &&
  Base.BaseDrops == BASES && Failed.FailedDrops == FAILURES ? 42 : 1; } }
'''
CASES = {
    'argument_throws': (1, 1, 0, 0, 0, 0),
    'base_throws': (2, 1, 1, 1, 0, 0),
    'terminal_throws': (3, 1, 2, 21, 1, 0),
    'outer_throws': (4, 1, 2, 21, 1, 0),
    'success': (0, 0, 2, 21, 1, 1),
}
CONSTRUCTIONS = {
    'direct': 'owned Failed value = new Failed();',
    'rc_factory': 'Rc<Failed> value = SharedPointers.MakeRcThrowing<Failed>();',
    'arc_factory': 'Arc<Failed> value = SharedPointers.MakeArcThrowing<Failed>();',
}

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    if len(sys.argv) not in (5, 6) or (len(sys.argv) == 6 and sys.argv[5] not in ('--compile', '--execute')):
        raise SystemExit('usage: shared_throwing_delegation_base.py <nsc> <Core.lib> <Core.nsmeta> <new-dir-on-A> [--compile|--execute]')
    global COMPILER, CORE, METADATA
    COMPILER, CORE, METADATA = (Path(p).resolve() for p in sys.argv[1:4])
    if any(p.drive.lower() != 'a:' for p in (COMPILER, CORE, METADATA)):
        raise SystemExit('compiler, SDK and metadata must be isolated on A:')
    root = Path(sys.argv[4]).resolve()
    if root.drive.lower() != 'a:':
        raise SystemExit('evidence directory must be on A:')
    root.mkdir(parents=True, exist_ok=False)
    mode = sys.argv[5] if len(sys.argv) == 6 else ''
    before = {str(path): digest(path) for path in (COMPILER, CORE, METADATA)}
    records = []
    for name, (stage, catches, payloads, trace, bases, failures) in CASES.items():
        for kind, construction in CONSTRUCTIONS.items():
            directory = root / (name + '-' + kind)
            directory.mkdir()
            source = TEMPLATE
            for key, value in [('MODE', stage), ('CONSTRUCT', construction), ('CATCHES', catches),
                               ('PAYLOADS', payloads), ('TRACE', trace), ('BASES', bases), ('FAILURES', failures)]:
                source = source.replace(key, str(value))
            (directory / 'Main.ns').write_text(source, encoding='utf-8')
            project = directory / 'Probe.nsproj'
            project.write_text('<Project><PropertyGroup><Name>DelegationBaseProbe</Name></PropertyGroup>'
                '<ItemGroup><Compile Include="Main.ns"/><LibraryReference Include="' + CORE.as_posix() +
                '" Metadata="' + METADATA.as_posix() + '"/></ItemGroup></Project>', encoding='utf-8')
            for level in ('O0', 'O2'):
                executable = directory / (level + '.exe')
                command = [str(COMPILER), '--emit-project-oop-exe', str(project), '-' + level,
                           '-o', str(executable), '--no-artifact-cache']
                record = {'case': name, 'construction': kind, 'level': level, 'command': command}
                records.append(record)
                if mode:
                    try:
                        build = subprocess.run(command, cwd=REPOSITORY, capture_output=True, text=True,
                                               errors='replace', timeout=120)
                        record.update(compile_exit=build.returncode, stdout=build.stdout, stderr=build.stderr)
                        if build.returncode == 0 and mode == '--execute':
                            run = subprocess.run([str(executable)], cwd=REPOSITORY, capture_output=True, text=True,
                                                 errors='replace', timeout=10)
                            record.update(run_exit=run.returncode, run_stdout=run.stdout, run_stderr=run.stderr)
                    except subprocess.TimeoutExpired:
                        record['timeout'] = True
                print(name, kind, level, record.get('compile_exit'), record.get('run_exit'), flush=True)
                (root / 'results.json').write_text(json.dumps({'before': before, 'records': records}, indent=2),
                                                    encoding='utf-8')
    after = {str(path): digest(path) for path in (COMPILER, CORE, METADATA)}
    (root / 'results.json').write_text(json.dumps({'before': before, 'after': after, 'records': records}, indent=2),
                                       encoding='utf-8')
    if before != after:
        return 2
    return int(bool(mode) and any(r.get('compile_exit') != 0 or
        (mode == '--execute' and r.get('run_exit') != 42) for r in records))

if __name__ == '__main__':
    sys.exit(main())
