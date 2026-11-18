"""A local owner must not escape through a direct ref or out assignment."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

compiler = Path(sys.argv[1]).resolve()
core = Path(os.environ['NSC_TEST_CORE_BINARY']).resolve()
failures = []
with tempfile.TemporaryDirectory(prefix='nsc-view-parameter-') as directory:
    root = Path(directory)
    project = root / 'Probe.nsproj'
    project.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup><ItemGroup>'
                       '<Compile Include="Main.ns"/><LibraryReference Include="' + core.as_posix() +
                       '" Metadata="' + core.with_suffix('.nsmeta').as_posix() + '"/></ItemGroup></Project>', encoding='utf-8')
    for family in ('Rc', 'Arc'):
        for mode in ('ref', 'out'):
            for safe in (False, True):
                parameters = 'SharedView<Payload> incoming, ' if safe else ''
                owner = '' if safe else family + '<Payload> owner = SharedPointers.Make' + family + '<Payload>(); '
                input_view = 'incoming' if safe else 'owner.Borrow()'
                body = owner + 'output = ' + input_view + ';'
                caller = (family + '<Payload> owner = SharedPointers.Make' + family + '<Payload>(); '
                          if safe else '')
                caller += 'SharedView<Payload> result = default(SharedView<Payload>); Sink.Forward('
                caller += ('owner.Borrow(), ' if safe else '') + mode + ' result); return result.Get().Data;'
                source = ('using NativeSharp.Core; public class Payload { public int Data; '
                          'public Payload() { Data = 42; } } public class Sink { '
                          'public static void Save((SharedView<Payload>, int) pair, ' + mode +
                          ' SharedView<Payload> output) { var (view, number) = pair; output = view; } '
                          'public static void Forward(' + parameters + mode + ' SharedView<Payload> output) { '
                          + body + ' } } class Program { public static int Main() { ' + caller + ' } }')
                (root / 'Main.ns').write_text(source, encoding='utf-8')
                command = [str(compiler), '--check-project-method-bodies', str(project)]
                result = subprocess.run(command, cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
                print('CASE', family, mode, 'safe' if safe else 'escape', '\nSOURCE', source,
                      '\nCOMMAND', subprocess.list2cmdline(command), 'EXIT', result.returncode,
                      result.stdout + result.stderr, flush=True)
                if (result.returncode == 0) != safe:
                    failures.append((family, mode, safe, result.returncode))
                elif not safe and 'view output cannot outlive its possible owner' not in result.stdout + result.stderr:
                    failures.append((family, mode, safe, 'wrong diagnostic'))
print('FAILURES', failures, flush=True)
sys.exit(bool(failures))
