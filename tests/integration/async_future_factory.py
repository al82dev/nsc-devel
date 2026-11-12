"""Await a synchronous future factory and retain direct async-call behavior."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile


SOURCE = '''class Counter { public static int Created; }
class Program {
 public static async int Slow(int value) { yield; return value; }
 public static async int Other() { yield; return 40; }
 public static async int Fast() { return 41; }
 public static Future<int> MakeFast() { Counter.Created = Counter.Created + 1; return Fast(); }
 public static Future<T> MakeGeneric<T>(Future<T> value) { return move(value); }
 public static Future<int> Make(int selector) {
  Counter.Created = Counter.Created + 1;
  if (selector == 0) { return Slow(41); }
  return Other();
 }
 public Future<int> MakeInstance() { return Make(0); }
 public static async int Main() { BODY }
}
'''

CASES = {
    'factory_direct': 'int value = await Make(0); return value + Counter.Created;',
    'factory_second_producer': 'int value = await Make(1); return value + Counter.Created + 1;',
    'factory_variable': 'Future<int> future = Make(0); int value = await future; return value + Counter.Created;',
    'factory_instance': 'Program program = new Program(); int value = await program.MakeInstance(); return value + Counter.Created;',
    'factory_generic': 'int value = await MakeGeneric<int>(Slow(41)); return value + 1;',
    'async_direct': 'int value = await Slow(41); return value + 1;',
    'factory_blocking': 'int value = await MakeFast(); return value + Counter.Created;',
}

NAMESPACE_SOURCE = '''namespace Helpers { public Future<int> Make() { return Program.Slow(41); } }
class Program { public static async int Slow(int value) { yield; return value; }
 public static async int Main() { int value = await Helpers.Make(); return value + 1; } }
'''


def main():
    if os.name != 'nt':
        return 77
    compiler = Path(sys.argv[1]).resolve()
    repo_root = Path(__file__).resolve().parents[2]
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-future-factory-') as temporary:
        root = Path(temporary)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup>'
                           '<ItemGroup><Compile Include="Main.ns" /></ItemGroup></Project>', encoding='utf-8')
        cases = dict(CASES)
        cases['namespace_factory'] = ''
        for name, body in cases.items():
            source = NAMESPACE_SOURCE if name == 'namespace_factory' else SOURCE.replace('BODY', body)
            if name == 'factory_blocking':
                source = source.replace('public static async int Main()', 'public static int Main()')
            (root / 'Main.ns').write_text(source, encoding='utf-8')
            for level in ('O0', 'O2'):
                executable = root / (name + '-' + level + '.exe')
                command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level,
                           '-o', str(executable), '--no-artifact-cache']
                result = subprocess.run(command, cwd=repo_root, capture_output=True,
                                        text=True, errors='replace', timeout=120)
                if result.returncode:
                    failures.append((name, level, 'compile', result.stdout + result.stderr))
                    continue
                result = subprocess.run([str(executable)], cwd=repo_root, capture_output=True,
                                        text=True, errors='replace', timeout=30)
                print(name, level, 'exit=' + str(result.returncode), flush=True)
                if result.returncode != 42:
                    failures.append((name, level, 'run', result.returncode))
    for failure in failures:
        print('FAIL', *failure, file=sys.stderr)
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
