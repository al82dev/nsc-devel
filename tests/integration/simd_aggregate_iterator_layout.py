"""Directed SIMD tuple/iterator layout checks; original reproductions stay intact."""
import ctypes
import os
from pathlib import Path
import subprocess
import sys

compiler = Path(sys.argv[1]).resolve()
root = Path(sys.argv[2]).resolve()
repository = Path(__file__).resolve().parents[2]
check_only = len(sys.argv) > 3 and sys.argv[3] == '--check-only'
if os.name != 'nt' or compiler.drive.upper() != 'A:' or root.drive.upper() != 'A:':
    raise SystemExit(2)
ctypes.windll.kernel32.SetErrorMode(0x0001 | 0x0002 | 0x8000)
root.mkdir(parents=True, exist_ok=False)
failures = []
for lanes in (4, 8, 16):
    simd = 'simd<int, ' + str(lanes) + '>'
    initializer = ('{ unsafe("prepare lane") { int* p = reinterpret_cast<int*>(&value); '
                   'p[0] = 17; p[' + str(lanes - 1) + '] = 25; } }')
    check = ('unsafe("inspect lane") { int* p = reinterpret_cast<int*>(&actual); '
             'if (p[0] != 17 || p[' + str(lanes - 1) + '] != 25) return 3; }')
    for protocol in ('async', 'sync', 'tuple', 'future', 'nested-mask'):
        directory = root / (protocol + '-' + str(lanes))
        directory.mkdir()
        if protocol == 'nested-mask':
            mask = 'mask<int, ' + str(lanes) + '>'
            source = ('class Program { public static int Main() { ' + simd + ' value = default(' + simd + '); '
                      + mask + ' m = value == value; ((' + mask + ', int), int) pair = ((m, 19), 23); '
                      'var (inner, n) = pair; var (actual, p) = inner; '
                      'if (!actual[0] || !actual[' + str(lanes - 1) + ']) return 3; return n + p; } }')
        elif protocol == 'tuple':
            source = ('class Program { public static int Main() { ' + simd + ' value = default(' + simd + '); '
                      + initializer + ' (' + simd + ', int) pair = (value, 42); '
                      'var (actual, n) = pair; ' + check + ' return n; } }')
        elif protocol == 'future':
            source = ('class Program { public static async ' + simd + ' Gen(' + simd + ' value) { '
                      'yield; return value; } public static async int Main() { ' + simd + ' value = default('
                      + simd + '); ' + initializer + ' ' + simd + ' actual = await Gen(value); '
                      + check + ' return 42; } }')
        else:
            iterator = ('AsyncIterator' if protocol == 'async' else 'Iterator') + '<' + simd + ', int>'
            advance = 'await it.MoveNextAsync()' if protocol == 'async' else 'it.MoveNext()'
            source = ('class Program { public static ' + iterator + ' Gen(' + simd + ' value) { '
                      'yield return value; yield return value; } public static int Main() { ' + simd +
                      ' value = default(' + simd + '); ' + initializer + ' ' + iterator +
                      ' it = Gen(value); bool next = ' + advance + '; '
                      'if (!next) return 1; ' + simd + ' actual = it.Current; ' + check +
                      ' next = ' + advance + '; if (!next) return 2; actual = it.Current; ' + check +
                      ' it.Drop(); return 42; } }')
        (directory / 'Main.ns').write_text(source, encoding='utf-8')
        project = directory / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup>'
                           '<ItemGroup><Compile Include="Main.ns" /></ItemGroup></Project>', encoding='utf-8')
        for level in (('MIR',) if check_only else ('O0', 'O2')):
            executable = directory / (level + '.exe')
            command = ([str(compiler), '--check-project-oop-mir', str(project)] if check_only else
                       [str(compiler), '--emit-project-oop-exe', str(project), '-' + level,
                        '-o', str(executable), '--no-artifact-cache'])
            result = subprocess.run(command, cwd=repository, capture_output=True, text=True,
                                    errors='replace', timeout=120)
            log = ('COMMAND ' + subprocess.list2cmdline(command) + '\nCWD ' + str(repository) +
                   '\nCOMPILE_EXIT ' + str(result.returncode) + '\n' + result.stdout + result.stderr)
            if result.returncode == 0 and not check_only:
                try:
                    run = subprocess.run([str(executable)], cwd=repository, capture_output=True,
                                         text=True, errors='replace', timeout=10)
                    log += 'RUN_EXIT ' + str(run.returncode) + '\n' + run.stdout + run.stderr
                    if run.returncode != 42:
                        failures.append((protocol, lanes, level, 'run', run.returncode))
                except subprocess.TimeoutExpired:
                    log += 'RUN_TIMEOUT\n'
                    failures.append((protocol, lanes, level, 'timeout'))
            elif result.returncode != 0:
                failures.append((protocol, lanes, level, 'compile', result.returncode))
            (directory / (level + '.log')).write_text(log, encoding='utf-8')
            print(protocol, lanes, level, 'compile=' + str(result.returncode), flush=True)
print('FAILURES', failures, flush=True)
raise SystemExit(int(bool(failures)))
