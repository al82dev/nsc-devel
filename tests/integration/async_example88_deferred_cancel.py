"""Example 88's original stream under the approved deferred-cancellation contract."""

import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


CONSUMER = '''namespace DeterministicFutureCancellation {
public class Program {
 public static async int Main() {
  CleanupTraceLog.Reset();
  AsyncIterator<int, int> stream = CancellableSessionWork.StreamWithTimeoutBudgetAsync();
  bool first = await stream.MoveNextAsync();
  if (!first || stream.Current != 700 || CleanupTraceLog.Value() != 0) return 10;
  stream.Cancel();
  if (CleanupTraceLog.Value() != 0) return 11;
  bool second = await stream.MoveNextAsync();
  if (second || CleanupTraceLog.Value() != 7) return 12;
  bool third = await stream.MoveNextAsync();
  if (third || CleanupTraceLog.Value() != 7) return 13;
  stream.Drop();
  if (CleanupTraceLog.Value() != 7) return 14;
  INSTRUMENTED_CHECK
  return 42;
 }
}
}
'''

TWO_LEASE_CONSUMER = '''namespace DeterministicFutureCancellation {
public class Program {
 public static async int Main() {
  CleanupTraceLog.Reset();
  AsyncIterator<int, int> stream = CancellableSessionWork.StreamWithTimeoutBudgetAsync();
  bool first = await stream.MoveNextAsync();
  if (!first || stream.Current != 700 || CleanupTraceLog.Value() != 0) return 20;
  bool second = await stream.MoveNextAsync();
  if (!second || stream.Current != 800 || CleanupTraceLog.Value() != 0) return 21;
  stream.Cancel();
  if (CleanupTraceLog.Value() != 0) return 22;
  bool third = await stream.MoveNextAsync();
  if (third || CleanupTraceLog.Value() != 87) return 23;
  bool fourth = await stream.MoveNextAsync();
  if (fourth || CleanupTraceLog.Value() != 87) return 24;
  stream.Drop();
  if (CleanupTraceLog.Value() != 87) return 25;
  INSTRUMENTED_CHECK
  return 42;
 }
}
}
'''


def main():
    if os.name != 'nt':
        return 77
    compiler = Path(sys.argv[1]).resolve()
    repository = Path(__file__).resolve().parents[2]
    fixture = Path(__file__).resolve().parent / 'fixtures' / 'example88_original' / 'src'
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-example88-cancel-') as temporary:
        root = Path(temporary)
        for name in ('CancellableSessionWork.ns', 'CleanupTraceLog.ns'):
            shutil.copyfile(fixture / name, root / name)
        original_guard = (fixture / 'ScopedChannelLease.ns').read_text(encoding='utf-8')
        class_anchor = '        private int leaseDigit_;'
        ctor_anchor = '            leaseDigit_ = leaseDigit;'
        if original_guard.count(class_anchor) != 1 or original_guard.count(ctor_anchor) != 1:
            raise ValueError('the preserved example no longer matches the instrumentation anchors')
        instrumented_guard = original_guard.replace(
            class_anchor, class_anchor + '\n        public static int Builds7;\n        public static int Builds8;')
        instrumented_guard = instrumented_guard.replace(
            ctor_anchor, ctor_anchor + '\n            if (leaseDigit == 7) Builds7 = Builds7 + 1;'
            '\n            if (leaseDigit == 8) Builds8 = Builds8 + 1;')
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup>'
                           '<ItemGroup><Compile Include="*.ns" /></ItemGroup></Project>', encoding='utf-8')
        for case, guard, consumer, check in (
            ('original_sources', original_guard, CONSUMER, ''),
            ('construction_counts', instrumented_guard, CONSUMER,
             'if (ScopedChannelLease.Builds7 != 1 || ScopedChannelLease.Builds8 != 0) return 15;'),
            ('two_leases_original_sources', original_guard, TWO_LEASE_CONSUMER, ''),
            ('two_leases_construction_counts', instrumented_guard, TWO_LEASE_CONSUMER,
             'if (ScopedChannelLease.Builds7 != 1 || ScopedChannelLease.Builds8 != 1) return 26;'),
        ):
            (root / 'ScopedChannelLease.ns').write_text(guard, encoding='utf-8')
            (root / 'Main.ns').write_text(consumer.replace('INSTRUMENTED_CHECK', check), encoding='utf-8')
            for level in ('O0', 'O2'):
                executable = root / (case + '-' + level + '.exe')
                command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level,
                           '-o', str(executable), '--no-artifact-cache']
                build = subprocess.run(command, cwd=repository, capture_output=True, text=True,
                                       errors='replace', timeout=120)
                if build.returncode:
                    failures.append((case, level, 'compile', build.returncode, build.stdout + build.stderr))
                    continue
                try:
                    run = subprocess.run([str(executable)], cwd=repository, capture_output=True,
                                         text=True, errors='replace', timeout=10)
                except subprocess.TimeoutExpired:
                    failures.append((case, level, 'timeout', 10))
                    continue
                print(case, level, 'exit=' + str(run.returncode), flush=True)
                if run.returncode != 42:
                    failures.append((case, level, 'run', run.returncode, run.stdout + run.stderr))
    for failure in failures:
        print('FAIL', *failure, file=sys.stderr)
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
