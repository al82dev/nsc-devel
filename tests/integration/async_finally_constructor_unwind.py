"""Check that only structurally proven constructors omit async unwind edges."""

from pathlib import Path
import re
import subprocess
import sys

import async_finally_exception_replacement as replacement
from isolated_a_runner import absolute_a, setup


def main():
    if len(sys.argv) != 3:
        raise SystemExit('expected nsc and a new evidence directory on A:')
    compiler, root = (absolute_a(value) for value in sys.argv[1:])
    repository = Path(__file__).resolve().parents[2]
    environment = setup(root, repository)
    safe = replacement.common.PREFIX.replace('BODY', replacement.common.CASES['async_suspended_replace'])
    unknown = '''using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception { public int Id; public Exception(int id) { Id = id; } } }
class Gate { static Gate() throws Exception { throw new Exception(9); } public static int Read() { return 1; } }
class ProbeValue { STATIC public int Id; public ProbeValue(int id) { BODY } }
class Program { public static int Log; public static async int Slow() { yield; return 1; }
 public static async int Work() throws Exception { try { ProbeValue value = new ProbeValue(4); return value.Id; }
  finally { int ignored = await Slow(); Log = 7; } }
 public static async int Main() { try { return await Work(); } catch (Exception error) { return error.Id; } }
}'''
    cases = {
        'safe': (safe, 'call', r'__nsc_ctor_Probe.*Exception__Exception__builtin\$3Aint', r'i32 1'),
        'static': (unknown.replace('STATIC', 'public static int Init = Gate.Read();').replace('BODY', 'Id = id;'),
                   'invoke', r'__nsc_ctor_Probe.*ProbeValue__ProbeValue__builtin\$3Aint', r'i32 4'),
        'call': (unknown.replace('STATIC', '').replace('BODY', 'Id = id; int check = Gate.Read();'),
                 'invoke', r'__nsc_ctor_Probe.*ProbeValue__ProbeValue__builtin\$3Aint', r'i32 4'),
        'base': (unknown.replace('class ProbeValue { STATIC', 'class Base { public Base() { } }\nclass ProbeValue : Base {')
                 .replace('BODY', 'Id = id;'),
                 'invoke', r'__nsc_ctor_Probe.*ProbeValue__ProbeValue__builtin\$3Aint', r'i32 4'),
    }
    failures = []
    for name, (source, instruction, symbol, argument) in cases.items():
        case = root / name
        case.mkdir()
        (case / 'Main.ns').write_text(source, encoding='utf-8')
        project = case / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup>'
                           '<ItemGroup><Compile Include="Main.ns"/></ItemGroup></Project>', encoding='utf-8')
        command = [str(compiler), '--dump-project-oop-llvm', str(project)]
        print('COMMAND', subprocess.list2cmdline(command), flush=True)
        result = subprocess.run(command, cwd=repository, env=environment, capture_output=True,
                                text=True, errors='replace', timeout=120)
        (case / 'llvm.log').write_text(result.stdout + result.stderr, encoding='utf-8')
        pattern = r'\b' + instruction + r' void @"' + symbol + r'"\([^\n]*' + argument + r'\)'
        if result.returncode or not re.search(pattern, result.stdout):
            failures.append((name, result.returncode, 'expected ' + instruction))
        else:
            print('PASS', name, instruction, flush=True)
    for failure in failures:
        print('FAIL', *failure, file=sys.stderr)
    return int(bool(failures))


if __name__ == '__main__':
    sys.exit(main())
