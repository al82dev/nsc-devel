"""Coordinate release of the last public WeakArc during payload destruction."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def run(command, cwd):
    result = subprocess.run([str(value) for value in command], cwd=cwd, capture_output=True,
                            text=True, errors='replace', timeout=120)
    print('COMMAND', subprocess.list2cmdline([str(value) for value in command]),
          'EXIT', result.returncode, result.stdout + result.stderr, flush=True)
    return result


def main():
    compiler = Path(sys.argv[1]).resolve()
    core_argument = sys.argv[2] if len(sys.argv) > 2 else os.environ.get('NSC_TEST_CORE_BINARY')
    if not core_argument:
        print('SKIP: set NSC_TEST_CORE_BINARY to the isolated Core library')
        return 77
    library = Path(core_argument).resolve()
    metadata = library.with_suffix('.nsmeta')
    if not library.is_file() or not metadata.is_file():
        raise ValueError('isolated Core library and metadata are required')
    asan_flags = []
    if os.environ.get('NSC_SHARED_LAST_WEAK_ASAN') == '1':
        clang = os.environ.get('NSC_TEST_ASAN_CLANG')
        runtime = os.environ.get('NSC_TEST_ASAN_RUNTIME')
        if not clang or not runtime:
            raise ValueError('ASan check requires NSC_TEST_ASAN_CLANG and NSC_TEST_ASAN_RUNTIME')
        asan_flags = ['--sanitize-address', '--asan-clang=' + clang, '--asan-runtime=' + runtime]
    repo = Path(__file__).resolve().parents[2]
    source = '''using NativeSharp.Core; using NativeSharp.Core.Threading;
class State { public static int Entered; public static int Released; public static int Drops;
 public static WeakArc<Payload> Weak;
 public static void ReleaseWeak() {
  while (Interlocked.Load(ref Entered) == 0) { }
  Weak = default(WeakArc<Payload>);
  Interlocked.Store(ref Released, 1);
 }
}
class Payload { public Payload() { } ~Payload() {
 Interlocked.Store(ref State.Entered, 1);
 while (Interlocked.Load(ref State.Released) == 0) { }
 State.Drops = State.Drops + 1;
} }
class Program { public static int Main() {
 int round = 0;
 while (round < 64) {
  State.Entered = 0; State.Released = 0;
  { Arc<Payload> owner = SharedPointers.MakeArc<Payload>();
    State.Weak = owner.Downgrade();
    ThreadStart start = State.ReleaseWeak; raii Thread worker = new Thread(start);
    { Arc<Payload> last = move(owner); }
    worker.Join();
  }
  if (State.Drops != round + 1 || !State.Weak.Expired()) return 1;
  round = round + 1;
 }
 return 42;
} }
'''
    with tempfile.TemporaryDirectory(prefix='nsc-arc-public-last-weak-') as temporary:
        root = Path(temporary)
        for name in ('Thread.ns', 'Atomics.ns'):
            shutil.copyfile(repo / 'lib' / 'NativeSharp.Core.Threading' / name, root / name)
        (root / 'Main.ns').write_text(source, encoding='utf-8')
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>ArcPublicLastWeak</Name></PropertyGroup>'
                           '<ItemGroup><Compile Include="*.ns" /><LibraryReference Include="' +
                           library.as_posix() + '" Metadata="' + metadata.as_posix() +
                           '" /></ItemGroup></Project>', encoding='utf-8')
        for level in ('O0', 'O2'):
            executable = root / ('last-weak-' + level + '.exe')
            build = run([compiler, '--emit-project-oop-exe', project, '-' + level,
                         '-o', executable, '--no-artifact-cache'] + asan_flags, root)
            if build.returncode:
                return 1
            result = run([executable], root)
            if result.returncode != 42 or 'ERROR: AddressSanitizer' in result.stdout + result.stderr:
                return 1
    return 0


if __name__ == '__main__':
    if os.name != 'nt':
        raise SystemExit('this directed test requires native Windows execution')
    sys.exit(main())
