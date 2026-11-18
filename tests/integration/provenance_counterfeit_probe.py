"""Control nominal: an unrelated homonymous type is not a borrowed view."""
from pathlib import Path
import subprocess
import sys
import tempfile

compiler, scratch = (Path(p).resolve() for p in sys.argv[1:3])
with tempfile.TemporaryDirectory(prefix='view-provenance-counterfeit-', dir=scratch) as directory:
    root = Path(directory)
    (root / 'Main.ns').write_text('''namespace NativeSharp.Core {
 public struct SharedView<T> { public int Number; } }
public class Payload { }
public class Sink {
 public static void Save((NativeSharp.Core.SharedView<Payload>, int) pair,
                         ref NativeSharp.Core.SharedView<Payload> output) {
  var (view, number) = pair; output = view;
 }
}''', encoding='utf-8')
    project = root / 'Producer.nsproj'
    project.write_text('<Project><PropertyGroup><Name>CounterfeitProducer</Name>'
        '<OutputType>Library</OutputType></PropertyGroup><ItemGroup><Compile Include="Main.ns"/>'
        '</ItemGroup></Project>', encoding='utf-8')
    command = [str(compiler), '--dump-project-method-bodies', str(project)]
    result = subprocess.run(command, cwd=Path(__file__).resolve().parents[2], capture_output=True, text=True, errors='replace', timeout=120)
    output = result.stdout + result.stderr
    print('COMMAND', subprocess.list2cmdline(command), 'EXIT', result.returncode, output, flush=True)
    if result.returncode or 'prototype-direct-view-producer unknown callable=' in output:
        raise SystemExit('counterfeit must analyze without becoming a borrowed-view candidate')
    print('PASS: homonymous type is not classified as a view', flush=True)
