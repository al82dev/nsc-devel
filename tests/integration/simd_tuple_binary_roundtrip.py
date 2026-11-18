"""Check SIMD and nested mask tuple values across NativeSharp binary modules."""
import ctypes
import os
from pathlib import Path
import subprocess
import sys

from isolated_a_runner import absolute_a, setup

if len(sys.argv) != 3:
    raise SystemExit('expected nsc and a new evidence directory on A:')
compiler, root = map(absolute_a, sys.argv[1:])
repository = Path(__file__).resolve().parents[2]
if os.name != 'nt':
    raise SystemExit(2)
ctypes.windll.kernel32.SetErrorMode(0x0001 | 0x0002 | 0x8000)
environment = setup(root, repository)
producer_source = '''public class Reader {
    public static int Read((simd<int, 4>, int) pair) {
        var (value, n) = pair;
        unsafe("inspect binary tuple") { int* p = reinterpret_cast<int*>(&value); return p[0] + p[3] + n; }
    }
    public static int ReadMask(((mask<int, 4>, int), int) pair) {
        var (inner, n) = pair; var (value, p) = inner;
        if (value[0] || !value[1] || value[3]) return 1;
        return n + p;
    }
}'''
consumer_source = '''class Program { public static int Main() {
    simd<int, 4> value = default(simd<int, 4>);
    unsafe("prepare binary tuple") { int* p = reinterpret_cast<int*>(&value); p[0] = 17; p[3] = 25; }
    mask<int, 4> m = value == default(simd<int, 4>);
    (simd<int, 4>, int) pair = (value, 0);
    ((mask<int, 4>, int), int) nested = ((m, 19), 23);
    if (Reader.Read(pair) != 42 || Reader.ReadMask(nested) != 42) return 1;
    return 42;
} }'''
failures = []

def call(label, command, expected=0):
    result = subprocess.run([str(item) for item in command], cwd=repository, env=environment,
                            capture_output=True, text=True, errors='replace', timeout=120)
    output = 'COMMAND ' + subprocess.list2cmdline([str(item) for item in command]) + '\n'
    output += 'EXIT ' + str(result.returncode) + '\n' + result.stdout + result.stderr
    (root / (label + '.log')).write_text(output, encoding='utf-8')
    print(label, result.returncode, flush=True)
    if result.returncode != expected:
        failures.append((label, result.returncode, expected))
    return result.returncode == expected

def project(folder, name, output, source, reference=''):
    folder.mkdir()
    (folder / 'Main.ns').write_text(source, encoding='utf-8')
    path = folder / 'Test.nsproj'
    path.write_text('<Project><PropertyGroup><Name>' + name + '</Name><OutputType>' + output +
                    '</OutputType></PropertyGroup><ItemGroup><Compile Include="Main.ns"/>' + reference +
                    '</ItemGroup></Project>', encoding='utf-8')
    return path

control = project(root / 'source', 'SourceControl', 'Exe', producer_source + '\n' + consumer_source)
producer = project(root / 'producer', 'TupleProducer', 'StaticLibrary', producer_source)
for producer_level in ('O0', 'O2'):
    source_exe = root / ('source-' + producer_level + '.exe')
    if call('source-build-' + producer_level,
            [compiler, '--emit-project-oop-exe', control, '-' + producer_level, '-o', source_exe,
             '--no-artifact-cache']):
        call('source-run-' + producer_level, [source_exe], 42)
    library = root / 'producer' / ('TupleProducer-' + producer_level + '.lib')
    if not call('producer-' + producer_level,
                [compiler, '--emit-project-oop-lib', producer, '-' + producer_level, '-o', library,
                 '--no-artifact-cache']):
        continue
    reference = '<LibraryReference Include="' + library.as_posix() + '" Metadata="' + \
                library.with_suffix('.nsmeta').as_posix() + '"/>'
    consumer = project(root / ('consumer-' + producer_level), 'TupleConsumer', 'Exe', consumer_source, reference)
    for consumer_level in ('O0', 'O2'):
        label = producer_level + '-' + consumer_level
        executable = root / ('consumer-' + label + '.exe')
        if call('consumer-build-' + label,
                [compiler, '--emit-project-oop-exe', consumer, '-' + consumer_level, '-o', executable,
                 '--no-artifact-cache']):
            call('consumer-run-' + label, [executable], 42)
print('FAILURES', failures)
raise SystemExit(int(bool(failures)))
