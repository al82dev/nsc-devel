import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def run(command, cwd):
    return subprocess.run([str(item) for item in command], cwd=cwd, capture_output=True,
                          text=True, errors='replace', timeout=120)


def main():
    compiler, *sources = map(Path, sys.argv[1:])
    program = '''using NativeSharp.Core; using NativeSharp.Core.Internal;
namespace NativeSharp.Core { public class Exception { } }
class Trace { public static long[] Block; public static long Observed; public static int Released; }
class Normal { ~Normal() { Trace.Observed = SharedAtomicControl.WeakCount(Trace.Block); } }
class ReleaseInside { ~ReleaseInside() {
 Trace.Released = SharedAtomicControl.DropWeak(Trace.Block);
 Trace.Observed = SharedAtomicControl.WeakCount(Trace.Block); } }
class Gate { static Gate() throws Exception { throw new Exception(); } public static int Read() { return 1; } }
class Failing { ~Failing() { Trace.Observed = SharedAtomicControl.WeakCount(Trace.Block); int x = Gate.Read(); } }
class Program { public static int Main() { unsafe("atomic reentrant weak count") {
 long[] block = SharedControlStorage.Allocate<Normal>();
 Normal value = SharedControlStorage.Construct<Normal>(block);
 Normal* slot = (Normal*)&block[6]; *slot = value; Trace.Block = block;
 SharedAtomicControl.Initialize(block);
 if (SharedAtomicControl.WeakCount(block) != 0l || !SharedAtomicControl.TryRetainWeak(block)) return 1;
 if (SharedAtomicControl.WeakCount(block) != 1l || SharedAtomicControl.DropStrong<Normal>(block, slot) != 1) return 2;
 if (Trace.Observed != 1l || SharedAtomicControl.WeakCount(block) != 1l ||
     SharedAtomicControl.DropWeak(block) != 1) return 3;
 block = SharedControlStorage.Allocate<ReleaseInside>();
 ReleaseInside second = SharedControlStorage.Construct<ReleaseInside>(block);
 ReleaseInside* secondSlot = (ReleaseInside*)&block[6]; *secondSlot = second; Trace.Block = block;
 SharedAtomicControl.Initialize(block);
 if (!SharedAtomicControl.TryRetainWeak(block) ||
     SharedAtomicControl.DropStrong<ReleaseInside>(block, secondSlot) != 1) return 4;
 if (Trace.Released != 0 || Trace.Observed != 0l) return 5;
 block = SharedControlStorage.Allocate<Failing>();
 Failing third = SharedControlStorage.Construct<Failing>(block);
 Failing* thirdSlot = (Failing*)&block[6]; *thirdSlot = third; Trace.Block = block;
 SharedAtomicControl.Initialize(block);
 if (!SharedAtomicControl.TryRetainWeak(block)) return 6;
 int caught = 0;
 try { SharedAtomicControl.DropStrong<Failing>(block, thirdSlot); }
 catch (Exception error) { caught = caught + 1; }
 if (caught != 1 || Trace.Observed != 1l || SharedAtomicControl.WeakCount(block) != 1l) return 7;
 return SharedAtomicControl.DropWeak(block) == 1 ? 42 : 8;
} } }
'''
    with tempfile.TemporaryDirectory(prefix='nsc-atomic-weak-reentrant-') as temporary:
        root = Path(temporary)
        for source in sources:
            shutil.copyfile(source, root / source.name)
        (root / 'Main.ns').write_text(program, encoding='utf-8')
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name><Language>3</Language></PropertyGroup>'
                           '<ItemGroup><Compile Include="*.ns" /></ItemGroup></Project>', encoding='utf-8')
        if os.name == 'nt':
            results = []
            for level in ('O0', 'O2'):
                exe = root / (level + '.exe')
                result = run([compiler, '--emit-project-oop-exe', project, '-o', exe, '-' + level], root)
                assert result.returncode == 0, (level, result.stdout + result.stderr)
                result = run([exe], root)
                results.append((level, result.returncode))
                print(level, 'exit=' + str(result.returncode), flush=True)
            assert all(code == 42 for _, code in results), results
    return 0


if __name__ == '__main__':
    sys.exit(main())
