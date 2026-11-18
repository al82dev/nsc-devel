"""Probe experimental producer observations; no consumer executable is run.

Arguments: compiler, preexisting Core library, existing scratch directory.
Requires a compiler built with the experimental dumper and callable merger.
This tests positive observations only, never completeness or verified contracts.
"""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

compiler, core, scratch = (Path(p).resolve() for p in sys.argv[1:4])
baseline = len(sys.argv) == 5 and sys.argv[4] == '--baseline'
with tempfile.TemporaryDirectory(prefix='view-provenance-producer-', dir=scratch) as name:
    root = Path(name)
    (root / 'Main.ns').write_text('''using NativeSharp.Core;
public class Payload { public int Data; public Payload() { Data = 42; } }
public class Sink {
 public static void Save((SharedView<Payload>, int) pair, ref SharedView<Payload> output) {
  var (view, number) = pair; output = view;
 }
 public static void SaveOut((SharedView<Payload>, int) pair, out SharedView<Payload> output) {
  var (view, number) = pair; output = view;
 }
 public static void NoTransfer((SharedView<Payload>, int) pair, out SharedView<Payload> output) {
  var (view, number) = pair; output = default(SharedView<Payload>);
 }
 public static void Forward((SharedView<Payload>, int) pair, out SharedView<Payload> output) {
  SaveOut(pair, out output);
 }
 public static int Read((SharedView<Payload>, int) pair) {
  var (view, number) = pair; return view.Get().Data + number;
 }
}''', encoding='utf-8')
    (root / 'Producer.nsproj').write_text(
        '<Project><PropertyGroup><Name>ProvenanceProducer</Name><OutputType>Library</OutputType>'
        '</PropertyGroup><ItemGroup><Compile Include="Main.ns"/><LibraryReference Include="' +
        core.as_posix() + '" Metadata="' + core.with_suffix('.nsmeta').as_posix() +
        '"/></ItemGroup></Project>', encoding='utf-8')
    command = [str(compiler), '--dump-project-method-bodies', str(root / 'Producer.nsproj')]
    result = subprocess.run(command, cwd=Path(__file__).resolve().parents[2], capture_output=True, text=True, errors='replace', timeout=120)
    print('COMMAND', subprocess.list2cmdline(command), 'EXIT', result.returncode,
          result.stdout + result.stderr, flush=True)
    if result.returncode:
        raise SystemExit('producer semantic analysis failed')
    edges = re.findall(r'^prototype-view-output unknown callable=(\d+) source=(\d+) output=(\d+)$',
                       result.stdout, re.MULTILINE)
    callables = {name: ordinal for ordinal, name in re.findall(
        r'^\s+\[(\d+)\].*?Sink\.(SaveOut|Save|Read|NoTransfer|Forward)\(', result.stdout, re.MULTILINE)}
    if set(callables) != {'Save', 'SaveOut', 'Read', 'NoTransfer', 'Forward'}:
        raise SystemExit('producer callable bindings missing: ' + repr(callables))
    if baseline:
        if edges:
            raise SystemExit('baseline unexpectedly has experimental observations: ' + repr(edges))
        print('BASELINE: no experimental observations; callable names below', flush=True)
        for line in result.stdout.splitlines():
            if re.match(r'^\s+\[\d+\].*Sink\.', line):
                print(line, flush=True)
        sys.exit(0)
    expected = {(callables['Save'], '0', '1'), (callables['SaveOut'], '0', '1')}
    if len(edges) != 2 or set(edges) != expected:
        raise SystemExit('expected two distinct unknown observations from Save and SaveOut: ' + repr(edges))
    candidates = re.findall(r'^prototype-direct-view-producer unknown callable=(\d+)$',
                            result.stdout, re.MULTILINE)
    if len(candidates) != 2 or set(candidates) != {callables['Save'], callables['SaveOut']}:
        raise SystemExit('unexpected producer candidates: ' + repr(candidates))
    print('PASS: two partial observations, no verified contract', flush=True)
