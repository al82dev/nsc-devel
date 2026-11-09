import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile


def run(command, cwd):
    return subprocess.run([str(item) for item in command], cwd=cwd, capture_output=True,
                          text=True, errors='replace', timeout=120)


def main():
    compiler, *sources = map(Path, sys.argv[1:])
    source = '''using NativeSharp.Core; using NativeSharp.Core.Internal;
namespace NativeSharp.Core { public class Exception { } }
class Trace { public static int Drops; }
KIND Payload { static Payload() throws Exception { throw new Exception(); }
 public Payload() { } ~Payload() { Trace.Drops = Trace.Drops + 1; } }
class Program { public static int Main() { int caught = 0;
 try { SharedHANDLEHandle<Payload> value = SharedFACTORYFactory.Create<Payload>(); }
 catch (Exception error) { caught = caught + 1; }
 return caught == 1 && Trace.Drops == 0 ? 42 : 1; } }
'''
    with tempfile.TemporaryDirectory(prefix='nsc-shared-static-cleanup-') as temporary:
        root = Path(temporary)
        for item in sources:
            shutil.copyfile(item, root / item.name)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>StaticFactoryProbe</Name><Language>3</Language>'
                           '</PropertyGroup><ItemGroup><Compile Include="*.ns" /></ItemGroup></Project>',
                           encoding='utf-8')
        for family, internal in (('Rc', 'Local'), ('Arc', 'Atomic')):
            for kind in ('class', 'struct'):
                program = source.replace('HANDLE', internal).replace('FACTORY', internal).replace('KIND', kind)
                (root / 'Main.ns').write_text(program, encoding='utf-8')
                result = run([compiler, '--dump-project-oop-llvm', project], root)
                assert result.returncode == 0, (family, kind, result.stdout + result.stderr)
                pattern = r'^define [^\n]*Shared' + internal + r'Factory__Create__g1__Payload[^\n]*\{\n(.*?)^\}'
                match = re.search(pattern, result.stdout, re.M | re.S)
                assert match, (family, kind, 'factory IR missing')
                body = match.group(1)
                invoke = re.search(r'invoke ptr [^\n]*SharedControlStorage__Construct[^\n]*\n'
                                   r'\s*to label %[^ ]+ unwind label %([^\s]+)', body)
                assert invoke, (family, kind, 'construction does not unwind through a cleanup')
                unwind = body.find('\n' + invoke.group(1) + ':')
                end = body.find('cleanupret ', unwind)
                cleanup = body[unwind:end] if unwind >= 0 and end >= 0 else ''
                assert 'cleanuppad' in cleanup and '@__nativesharp_free' in cleanup and '"funclet"' in cleanup, (
                    family, kind, 'exceptional cleanup does not release the block')
                if os.name == 'nt':
                    for level in ('O0', 'O2'):
                        exe = root / (family + '-' + kind + '-' + level + '.exe')
                        result = run([compiler, '--emit-project-oop-exe', project, '-o', exe, '-' + level], root)
                        assert result.returncode == 0, (family, kind, level, result.stdout + result.stderr)
                        result = run([exe], root)
                        assert result.returncode == 42, (family, kind, level, result.returncode)
                        print(family, kind, level, 'exit=42; cleanup IR verified', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
