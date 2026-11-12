"""Check shared control cleanup when a payload destructor throws."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def main():
    compiler = Path(sys.argv[1]).resolve()
    core = Path(sys.argv[2]).resolve()
    failures = []
    sources = ('SharedStorage.ns', 'SharedControlStorage.ns', 'SharedReferenceCounts.ns',
               'SharedLocalControl.ns', 'SharedAtomicControl.ns')
    with tempfile.TemporaryDirectory(prefix='nsc-shared-throwing-drop-') as temporary:
        root = Path(temporary)
        for name in sources:
            shutil.copyfile(core / name, root / name)
        (root / 'Probe.nsproj').write_text(
            '<Project><PropertyGroup><Name>SharedThrowingDrop</Name></PropertyGroup>'
            '<ItemGroup><Compile Include="*.ns"/></ItemGroup></Project>', encoding='utf-8')
        for family in ('Local', 'Atomic'):
            for kind in ('class', 'struct'):
                storage = ('long[] block = SharedControlStorage.Allocate<Payload>(); '
                           'Payload value = SharedControlStorage.Construct<Payload>(block); '
                           'Payload* slot = (Payload*)&block[6]; *slot = value;' if kind == 'class' else
                           'long[] block = SharedControlStorage.AllocateValue<Payload>(); '
                           'Payload* slot = SharedControlStorage.ConstructValue<Payload>(block);')
                program = '''using NativeSharp.Core; using NativeSharp.Core.Internal;
namespace NativeSharp.Core { public class Exception { } }
class Trace { public static int PayloadDrops; public static int ResourceDrops; }
class Resource { ~Resource() { Trace.ResourceDrops = Trace.ResourceDrops + 1; } }
class Gate { static Gate() throws Exception { throw new Exception(); } public static int Read() { return 1; } }
KIND Payload { private owned Resource item; public Payload() { item = new Resource(); }
 ~Payload() { Trace.PayloadDrops = Trace.PayloadDrops + 1; int ignored = Gate.Read(); } }
class Program { public static int Main() { unsafe("destructor field cleanup probe") {
 STORAGE
 SharedFAMILYControl.Initialize(block); SharedFAMILYControl.TryRetainWeak(block);
 int caught = 0; try { SharedFAMILYControl.DropStrong<Payload>(block, slot); }
 catch (Exception error) { caught = caught + 1; }
 if (caught != 1) return 11; if (Trace.PayloadDrops != 1) return 12;
 if (Trace.ResourceDrops != 1) return 13; if (SharedFAMILYControl.WeakCount(block) != 1l) return 14;
 if (SharedFAMILYControl.DropWeak(block) != 1) return 15; return 42;
} } }
'''.replace('FAMILY', family).replace('KIND', kind).replace('STORAGE', storage)
                resource = 'class Resource { ~Resource() { Trace.ResourceDrops = Trace.ResourceDrops + 1; } }'
                throwing_resource = ('class Gate2 { static Gate2() throws Exception { throw new Exception(); } '
                                     'public static int Read() { return 1; } } '
                                     'class Resource { ~Resource() { Trace.ResourceDrops = Trace.ResourceDrops + 1; '
                                     'int ignored = Gate2.Read(); } }')
                double = program.replace(resource, throwing_resource)
                if double == program:
                    raise AssertionError('resource fixture not found')
                for scenario, source, expected in (('single', program, 42), ('double', double, 148)):
                    (root / 'Main.ns').write_text(source, encoding='utf-8')
                    for level in ('O0', 'O2'):
                        executable = root / (family + '-' + kind + '-' + scenario + '-' + level + '.exe')
                        command = [str(compiler), '--emit-project-oop-exe', str(root / 'Probe.nsproj'),
                                   '-' + level, '-o', str(executable), '--no-artifact-cache']
                        result = subprocess.run(command, cwd=root, capture_output=True, text=True,
                                                errors='replace', timeout=120)
                        print(family, kind, scenario, level, 'COMMAND', subprocess.list2cmdline(command),
                              'EXIT', result.returncode, result.stdout + result.stderr, flush=True)
                        if result.returncode:
                            failures.append((family, kind, scenario, level, 'compile', result.returncode))
                            continue
                        result = subprocess.run([str(executable)], cwd=root, capture_output=True, text=True,
                                                errors='replace', timeout=120)
                        print(family, kind, scenario, level, 'RUN EXIT', result.returncode,
                              result.stdout + result.stderr, flush=True)
                        if result.returncode != expected:
                            failures.append((family, kind, scenario, level, 'execute', result.returncode))
    if failures:
        print('FAIL', failures, flush=True)
        return 1
    print('shared_destructor_throw_fields: PASS', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
