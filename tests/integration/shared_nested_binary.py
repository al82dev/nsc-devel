"""Verify nested shared handles across separately compiled NativeSharp libraries."""

from pathlib import Path
import shutil
import subprocess
import sys

from isolated_a_runner import absolute_a, setup
from shared_weak_readonly_binary import NAMES


SOURCE = '''using NativeSharp.Core;
using ProducerSide;
namespace Right { struct Value { public long right; public Value() { right = 22l; } } }
class Program {
 public static int Main() {
  STRONG<STRONG<ProducerSide.Value>> left = SharedPointers.MAKE<STRONG<ProducerSide.Value>>();
  STRONG<STRONG<Right.Value>> right = SharedPointers.MAKE<STRONG<Right.Value>>();
  STRONG<ProducerSide.Value> lvalue = SharedPointers.MAKE<ProducerSide.Value>();
  STRONG<Right.Value> rvalue = SharedPointers.MAKE<Right.Value>();
  if (lvalue.Get().left != 11 || rvalue.Get().right != 22l) return 3;
  if (left.IsEmpty() || right.IsEmpty() || !left.Get().IsEmpty() || !right.Get().IsEmpty()) return 1;
  WEAK<STRONG<ProducerSide.Value>> weak = left.Downgrade();
  STRONG<STRONG<ProducerSide.Value>> clone = weak.Upgrade();
  if (clone.IsEmpty() || clone.StrongCount() != 2l || !clone.Get().IsEmpty()) return 2;
  return 42;
 }
}'''


def execute(command, repository, environment):
    print('CWD', repository, 'COMMAND', subprocess.list2cmdline(command), flush=True)
    return subprocess.run(command, cwd=repository, env=environment, capture_output=True,
                          text=True, errors='replace', timeout=120)


def main():
    if len(sys.argv) != 3:
        raise SystemExit('expected nsc and new evidence directory on A:')
    compiler, root = (absolute_a(value) for value in sys.argv[1:])
    repository = Path(__file__).resolve().parents[2]
    environment = setup(root, repository)
    producer = root / 'producer'
    producer.mkdir()
    for name in NAMES:
        shutil.copyfile(repository / 'lib/NativeSharp.Core' / (name + '.ns'), producer / (name + '.ns'))
    (producer / 'Contracts.ns').write_text(
        'namespace NativeSharp.Core { public class Exception { } public trait Send {} public trait Sync {} }\n'
        'namespace ProducerSide { public struct Value { public int left; public Value() { left = 11; } } }',
        encoding='utf-8')
    project = producer / 'Core.nsproj'
    project.write_text('<Project><PropertyGroup><Name>NativeSharpCore</Name>'
        '<OutputType>StaticLibrary</OutputType></PropertyGroup>'
        '<ItemGroup><Compile Include="*.ns"/></ItemGroup></Project>', encoding='utf-8')
    failures = []
    for producer_level in ('O0', 'O2'):
        library = producer / ('Core-' + producer_level + '.lib')
        result = execute([str(compiler), '--emit-project-oop-lib', str(project), '-' + producer_level,
                          '-o', str(library), '--no-artifact-cache'], repository, environment)
        if result.returncode:
            failures.append((producer_level, 'producer', result.stdout + result.stderr))
            continue
        for strong, weak in (('Rc', 'WeakRc'), ('Arc', 'WeakArc')):
            case = root / (producer_level + '-' + strong)
            case.mkdir()
            (case / 'Main.ns').write_text(SOURCE.replace('STRONG', strong).replace('WEAK', weak)
                .replace('MAKE', 'Make' + strong), encoding='utf-8')
            consumer = case / 'Probe.nsproj'
            consumer.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup>'
                '<ItemGroup><Compile Include="Main.ns"/><LibraryReference Include="' + library.as_posix() +
                '" Metadata="' + library.with_suffix('.nsmeta').as_posix() + '"/></ItemGroup></Project>',
                encoding='utf-8')
            for level in ('O0', 'O2'):
                executable = case / (level + '.exe')
                result = execute([str(compiler), '--emit-project-oop-exe', str(consumer), '-' + level,
                                  '-o', str(executable), '--no-artifact-cache'], repository, environment)
                if result.returncode:
                    failures.append((producer_level, strong, level, 'compile', result.stdout + result.stderr))
                    continue
                result = execute([str(executable)], repository, environment)
                print('RUN', producer_level, strong, level, result.returncode, flush=True)
                if result.returncode != 42:
                    failures.append((producer_level, strong, level, 'run', result.returncode))
    for failure in failures:
        print('FAIL', *failure, file=sys.stderr)
    return int(bool(failures))


if __name__ == '__main__':
    sys.exit(main())
