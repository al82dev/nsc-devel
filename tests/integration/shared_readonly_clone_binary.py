"""Exercise shared-handle operations through in parameters across libraries."""

from pathlib import Path
import shutil
import subprocess
import sys

from isolated_a_runner import absolute_a, setup
from shared_weak_readonly_binary import NAMES


SOURCE = '''using NativeSharp.Core;
class Payload { public static int Drops; public int Value;
 public Payload() { Value = 42; } ~Payload() { Drops = Drops + 1; } }
class Program {
 static bool Empty(in STRONG<Payload> input) { return input.IsEmpty(); }
 static STRONG<Payload> Copy(in STRONG<Payload> input) { return input.Clone(); }
 static WEAK<Payload> Down(in STRONG<Payload> input) { return input.Downgrade(); }
 static WEAK<Payload> CopyWeak(in WEAK<Payload> input) { return input.Clone(); }
 static STRONG<Payload> Promote(in WEAK<Payload> input) { return input.Upgrade(); }
 public static int Main() {
  STRONG<Payload> empty = default(STRONG<Payload>);
  if (!Empty(in empty)) return 8;
  WEAK<Payload> emptyWeak = Down(in empty);
  { STRONG<Payload> emptyCopy = Copy(in empty);
    STRONG<Payload> emptyUpgrade = Promote(in emptyWeak);
    WEAK<Payload> emptyWeakCopy = CopyWeak(in emptyWeak);
    if (!emptyCopy.IsEmpty() || !emptyUpgrade.IsEmpty() || !emptyWeakCopy.Expired()) return 1; }
  WEAK<Payload> weak = default(WEAK<Payload>);
  WEAK<Payload> weakCopy = default(WEAK<Payload>);
  STRONG<Payload> copy = default(STRONG<Payload>);
  { STRONG<Payload> owner = SharedPointers.MAKE<Payload>();
    if (Empty(in owner)) return 9;
    copy = Copy(in owner); weak = Down(in owner); weakCopy = CopyWeak(in weak);
    if (owner.StrongCount() != 2l || weak.WeakCount() != 2l) return 2; }
  if (Payload.Drops != 0 || copy.Get().Value != 42 || weak.Expired()) return 3;
  { STRONG<Payload> promoted = Promote(in weakCopy);
    if (promoted.IsEmpty() || promoted.Get().Value != 42 || copy.StrongCount() != 2l) return 4; }
  { STRONG<Payload> last = move(copy); }
  if (Payload.Drops != 1 || !weak.Expired() || weak.WeakCount() != 2l) return 5;
  { STRONG<Payload> expired = Promote(in weak); WEAK<Payload> extra = CopyWeak(in weakCopy);
    if (!expired.IsEmpty() || !extra.Expired() || extra.WeakCount() != 3l) return 6; }
  return weak.WeakCount() == 2l && Payload.Drops == 1 ? 42 : 7;
 }
}'''

READONLY_NEGATIVE = '''using NativeSharp.Core;
class Payload { public int Value; public Payload() { Value = 42; } }
class Program { static void Mutate(in STRONG<Payload> input) {
 STRONG<Payload> copy = input.Clone(); copy.Get().Value = 7;
} public static int Main() { return 0; } }'''


def main():
    if len(sys.argv) != 3:
        raise SystemExit('expected nsc and new evidence directory on A:')
    compiler, root = (absolute_a(value) for value in sys.argv[1:])
    repository = Path(__file__).resolve().parents[2]
    environment = setup(root, repository)
    producer = root / 'producer'
    producer.mkdir()
    sources = repository / 'lib/NativeSharp.Core'
    for name in NAMES:
        shutil.copyfile(sources / (name + '.ns'), producer / (name + '.ns'))
    (producer / 'Contracts.ns').write_text(
        'namespace NativeSharp.Core { public class Exception { } public trait Send {} public trait Sync {} }',
        encoding='utf-8')
    project = producer / 'Core.nsproj'
    project.write_text('<Project><PropertyGroup><Name>NativeSharpCore</Name>'
        '<OutputType>StaticLibrary</OutputType></PropertyGroup>'
        '<ItemGroup><Compile Include="*.ns"/></ItemGroup></Project>', encoding='utf-8')
    failures = []
    for producer_level in ('O0', 'O2'):
        library = producer / ('Core-' + producer_level + '.lib')
        command = [str(compiler), '--emit-project-oop-lib', str(project), '-' + producer_level,
                   '-o', str(library), '--no-artifact-cache']
        print('CWD', repository, 'COMMAND', subprocess.list2cmdline(command), flush=True)
        result = subprocess.run(command, cwd=repository, env=environment, capture_output=True,
                                text=True, errors='replace', timeout=120)
        if result.returncode:
            failures.append((producer_level, 'producer', result.stdout + result.stderr))
            continue
        for strong, weak in (('Rc', 'WeakRc'), ('Arc', 'WeakArc')):
            case = root / (producer_level + '-' + strong)
            case.mkdir()
            (case / 'Main.ns').write_text(SOURCE.replace('STRONG', strong).replace('WEAK', weak)
                .replace('MAKE', 'Make' + strong), encoding='utf-8')
            consumer = case / 'Probe.nsproj'
            consumer.write_text('<Project><PropertyGroup><Name>ReadonlyShared</Name></PropertyGroup>'
                '<ItemGroup><Compile Include="Main.ns"/><LibraryReference Include="' + library.as_posix() +
                '" Metadata="' + library.with_suffix('.nsmeta').as_posix() + '"/></ItemGroup></Project>',
                encoding='utf-8')
            for level in ('O0', 'O2'):
                executable = case / (level + '.exe')
                command = [str(compiler), '--emit-project-oop-exe', str(consumer), '-' + level,
                           '-o', str(executable), '--no-artifact-cache']
                print('CWD', repository, 'COMMAND', subprocess.list2cmdline(command), flush=True)
                result = subprocess.run(command, cwd=repository, env=environment, capture_output=True,
                                        text=True, errors='replace', timeout=120)
                if result.returncode:
                    failures.append((producer_level, strong, level, 'compile', result.stdout + result.stderr))
                    continue
                result = subprocess.run([str(executable)], cwd=repository, env=environment,
                                        capture_output=True, text=True, errors='replace', timeout=10)
                print('RUN', producer_level, strong, level, result.returncode, flush=True)
                if result.returncode != 42:
                    failures.append((producer_level, strong, level, 'run', result.returncode, result.stdout + result.stderr))
            (case / 'Main.ns').write_text(READONLY_NEGATIVE.replace('STRONG', strong), encoding='utf-8')
            command = [str(compiler), '--check-project-method-bodies', str(consumer)]
            print('CWD', repository, 'COMMAND', subprocess.list2cmdline(command), flush=True)
            result = subprocess.run(command, cwd=repository, env=environment, capture_output=True,
                                    text=True, errors='replace', timeout=120)
            output = result.stdout + result.stderr
            print('REJECT', producer_level, strong, result.returncode, flush=True)
            if result.returncode == 0 or 'readonly' not in output.lower():
                failures.append((producer_level, strong, 'readonly-negative', result.returncode, output))
    for failure in failures:
        print('FAIL', *failure, file=sys.stderr)
    return int(bool(failures))


if __name__ == '__main__':
    sys.exit(main())
