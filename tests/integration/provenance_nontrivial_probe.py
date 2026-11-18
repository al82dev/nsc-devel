"""Check that tuples with destructor-bearing members are not candidates.

The compiler may reject deconstruction earlier; that is recorded separately.
Never executes producer code. Pass compiler and an existing scratch dir on A:.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

compiler, scratch = (Path(value).resolve() for value in sys.argv[1:3])
baseline = len(sys.argv) == 4 and sys.argv[3] == '--baseline'
with tempfile.TemporaryDirectory(prefix='view-provenance-drop-', dir=scratch) as directory:
    root = Path(directory)
    project = root / 'Producer.nsproj'
    project.write_text('<Project><PropertyGroup><Name>DropProducer</Name>'
        '<OutputType>Library</OutputType></PropertyGroup><ItemGroup><Compile Include="Main.ns"/>'
        '</ItemGroup></Project>', encoding='utf-8')
    for name, second in [('direct', 'Tracked'), ('nested', 'Wrapper')]:
        source = '''public borrowed struct V { public int Value; }
public class State { public static int Value; }
public struct Tracked { ~Tracked() { State.Value = 1; } }
public struct Wrapper { public Tracked Item; }
public class Sink {
 public static void Save((V, SECOND) pair, ref V output) {
  var (view, number) = pair; output = view;
 }
}'''.replace('SECOND', second)
        (root / 'Main.ns').write_text(source, encoding='utf-8')
        command = [str(compiler), '--dump-project-method-bodies', str(project)]
        result = subprocess.run(command, cwd=Path(__file__).resolve().parents[2], capture_output=True, text=True, errors='replace', timeout=120)
        output = result.stdout + result.stderr
        print('CASE', name, 'COMMAND', subprocess.list2cmdline(command), 'EXIT', result.returncode,
              output, flush=True)
        if result.returncode:
            if 'deconstruction declaration of owning or move-only elements requires' not in output:
                raise SystemExit('INCONCLUSIVE: unexpected diagnostic for ' + name)
            print('REJECTED BEFORE CANDIDATE', name, flush=True)
        elif 'prototype-direct-view-producer unknown callable=' in output:
            raise SystemExit('nontrivial tuple incorrectly became a candidate: ' + name)
        elif baseline:
            print('ACCEPTED; CANDIDATE CLASSIFICATION NOT AVAILABLE IN BASELINE', name, flush=True)
        else:
            print('ACCEPTED BUT NOT A CANDIDATE', name, flush=True)
