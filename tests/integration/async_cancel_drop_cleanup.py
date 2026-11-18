"""A pending cancellation still runs cleanup when the iterator is dropped."""

import importlib.util
import os
from pathlib import Path
import subprocess
import sys
import tempfile


GENERATOR = '''public static AsyncIterator<int, Exception> Gen() {
 try { owned Guard g = new Guard(); yield return 5; yield return 6; }
 finally { Counter.Log = Counter.Log + 1; }
} '''

MAIN = '''public static async int Main() { try { { AsyncIterator<int, Exception> it = Gen();
 bool first = await it.MoveNextAsync(); if (!first || it.Current != 5 || Counter.Builds != 1) return 20;
 BEFORE
 CANCEL
 if (Counter.Log != 0 || Counter.Drops != 0) return 21;
 ADVANCE
 it.Drop(); if (Counter.Log != 1 || Counter.Drops != 1) return 22;
 it.Drop(); if (Counter.Log != 1 || Counter.Drops != 1) return 23; }
 return Counter.Builds == 1 && Counter.Drops == 1 && Counter.Log == 1 ? 42 : 24;
 } catch (Exception error) { return 29; } }'''


def main():
    if os.name != 'nt':
        return 77
    compiler = Path(sys.argv[1]).resolve()
    repository = Path(__file__).resolve().parents[2]
    sys.dont_write_bytecode = True
    source = Path(__file__).with_name('async_generator_finally_exception.py')
    spec = importlib.util.spec_from_file_location('async_finally_fixture', source)
    fixture = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(fixture)
    prefix = fixture.PREFIX.replace('class Counter { public static int Log; }',
        '''class Counter { public static int Log; public static int Builds; public static int Drops; }
class Guard { public Guard() { Counter.Builds = Counter.Builds + 1; }
 ~Guard() { Counter.Drops = Counter.Drops + 1; } }''')
    cases = {
        'drop_direct': ('', '', ''),
        'cancel_then_drop': ('', 'it.Cancel();', ''),
        'cancel_twice_then_drop': ('', 'it.Cancel(); it.Cancel();', ''),
        'cancel_second_yield_drop': (
            'bool second = await it.MoveNextAsync(); if (!second || it.Current != 6) return 26;',
            'it.Cancel();', ''),
        'cancel_then_advance_drop': ('', 'it.Cancel();',
            'bool next = await it.MoveNextAsync(); if (next) return 25;'),
    }
    bodies = {name: GENERATOR + MAIN.replace('BEFORE', before).replace('CANCEL', cancel).replace('ADVANCE', advance)
              for name, (before, cancel, advance) in cases.items()}
    bodies['cancel_implicit_drop'] = GENERATOR + '''public static async int Main() { try {
 { AsyncIterator<int, Exception> it = Gen(); bool first = await it.MoveNextAsync();
 if (!first || it.Current != 5) return 20; it.Cancel();
 if (Counter.Log != 0 || Counter.Drops != 0) return 21; }
 return Counter.Builds == 1 && Counter.Drops == 1 && Counter.Log == 1 ? 42 : 22;
 } catch (Exception error) { return 29; } }'''
    bodies['drop_unstarted'] = GENERATOR + '''public static async int Main() { try {
 { AsyncIterator<int, Exception> it = Gen(); it.Drop(); it.Drop(); }
 return Counter.Builds == 0 && Counter.Drops == 0 && Counter.Log == 0 ? 42 : 22;
 } catch (Exception error) { return 29; } }'''
    bodies['drop_completed'] = GENERATOR + '''public static async int Main() { try {
 { AsyncIterator<int, Exception> it = Gen(); bool first = await it.MoveNextAsync();
 bool second = await it.MoveNextAsync(); bool last = await it.MoveNextAsync();
 if (!first || !second || last || Counter.Builds != 1 || Counter.Drops != 1 || Counter.Log != 1) return 20;
 it.Drop(); it.Drop(); }
 return Counter.Builds == 1 && Counter.Drops == 1 && Counter.Log == 1 ? 42 : 22;
 } catch (Exception error) { return 29; } }'''
    bodies['drop_after_error'] = '''public static AsyncIterator<int, Exception> Gen() throws Exception {
 try { owned Guard g = new Guard(); yield return 5; throw new Exception(1); }
 finally { Counter.Log = Counter.Log + 1; }
} public static async int Main() { int caught = 0;
 { AsyncIterator<int, Exception> it = Gen();
 try { bool first = await it.MoveNextAsync(); if (!first || it.Current != 5) return 20;
  bool next = await it.MoveNextAsync(); return 21; }
 catch (Exception error) { caught = error.Id; }
 it.Drop(); it.Drop(); }
 return caught == 1 && Counter.Builds == 1 && Counter.Drops == 1 && Counter.Log == 1 &&
  Exception.Builds == 1 && Exception.Drops == 1 ? 42 : 22; }'''
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-cancel-drop-') as temporary:
        root = Path(temporary)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup>'
                           '<ItemGroup><Compile Include="Main.ns" /></ItemGroup></Project>', encoding='utf-8')
        for name, body in bodies.items():
            (root / 'Main.ns').write_text(prefix.replace('BODY', body), encoding='utf-8')
            for level in ('O0', 'O2'):
                executable = root / (name + '-' + level + '.exe')
                command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level,
                           '-o', str(executable), '--no-artifact-cache']
                build = subprocess.run(command, cwd=repository, capture_output=True, text=True,
                                       errors='replace', timeout=120)
                if build.returncode:
                    failures.append((name, level, 'compile', build.returncode, build.stdout + build.stderr))
                    continue
                try:
                    run = subprocess.run([str(executable)], cwd=repository, capture_output=True,
                                         text=True, errors='replace', timeout=10)
                except subprocess.TimeoutExpired:
                    failures.append((name, level, 'timeout'))
                    continue
                print(name, level, 'exit=' + str(run.returncode), flush=True)
                if run.returncode != 42:
                    failures.append((name, level, 'run', run.returncode, run.stdout + run.stderr))
    for failure in failures:
        print('FAIL', *failure, file=sys.stderr)
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
