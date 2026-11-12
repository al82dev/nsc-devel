"""Cancellation must unwind nested finally blocks without yielding another item."""

import importlib.util
import os
from pathlib import Path
import subprocess
import sys
import tempfile


NESTED_CANCEL_FIRST = '''public static AsyncIterator<int, Exception> Gen() throws Exception {
 try { try { throw new Exception(1); } finally { yield return 5; } } finally { yield return 6; }
} public static async int Main() { try { { AsyncIterator<int, Exception> it = Gen();
 bool first = await it.MoveNextAsync(); if (!first || it.Current != 5) return 20;
 it.Cancel(); if (Exception.Drops != 0) return 21;
 bool next = await it.MoveNextAsync(); if (next) return 22; }
 return Exception.Builds == 1 && Exception.Drops == 1 && Exception.Log == 1 ? 42 : 23;
 } catch (Exception error) { return 29; } }'''


def main():
    if os.name != 'nt':
        return 77
    compiler = Path(sys.argv[1]).resolve()
    repository = Path(__file__).resolve().parents[2]
    temporary_base = Path(os.environ['TEMP']).resolve()
    if temporary_base.drive.upper() != 'A:':
        raise RuntimeError('TEMP must be on A:')
    sys.dont_write_bytecode = True
    source = Path(__file__).with_name('async_generator_finally_exception.py')
    spec = importlib.util.spec_from_file_location('async_finally_fixture', source)
    fixture = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(fixture)
    cases = {
        'nested_cancel_first': NESTED_CANCEL_FIRST,
        'nested_normal': fixture.CASES['nested_yields_exception'],
    }
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-nested-cancel-', dir=temporary_base) as temporary:
        root = Path(temporary)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup>'
                           '<ItemGroup><Compile Include="Main.ns" /></ItemGroup></Project>', encoding='utf-8')
        for name, body in cases.items():
            (root / 'Main.ns').write_text(fixture.PREFIX.replace('BODY', body), encoding='utf-8')
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
