"""An output view cannot borrow an owner consumed by the current function."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

compiler = Path(sys.argv[1]).resolve()
core = Path(os.environ['NSC_TEST_CORE_BINARY']).resolve()
failures = []
with tempfile.TemporaryDirectory(prefix='nsc-owned-parameter-view-') as directory:
    root = Path(directory)
    project = root / 'Probe.nsproj'
    project.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup><ItemGroup>'
                       '<Compile Include="Main.ns"/><LibraryReference Include="' + core.as_posix() +
                       '" Metadata="' + core.with_suffix('.nsmeta').as_posix() + '"/></ItemGroup></Project>', encoding='utf-8')
    for family in ('Rc', 'Arc'):
        for mode in ('ref', 'out'):
            for route in ('direct', 'forward'):
                for safe in (False, True):
                    incoming = 'SharedView<Payload> input' if safe else family + '<Payload> input'
                    view = 'input' if safe else 'input.Borrow()'
                    body = ('output = ' + view + ';' if route == 'direct' else
                            'Sink.Save((' + view + ', 0), ' + mode + ' output);')
                    argument = 'owner.Borrow()' if safe else 'move(owner)'
                    source = ('using NativeSharp.Core; public class Payload { public int Data; '
                              'public Payload() { Data = 42; } } public class Sink { '
                              'public static void Save((SharedView<Payload>, int) pair, ' + mode +
                              ' SharedView<Payload> output) { var (view, number) = pair; output = view; } '
                              'public static void Forward(' + incoming + ', ' + mode +
                              ' SharedView<Payload> output) { ' + body + ' } } '
                              'class Program { public static int Main() { ' + family +
                              '<Payload> owner = SharedPointers.Make' + family + '<Payload>(); '
                              'SharedView<Payload> result = default(SharedView<Payload>); Sink.Forward(' +
                              argument + ', ' + mode + ' result); return result.Get().Data; } }')
                    (root / 'Main.ns').write_text(source, encoding='utf-8')
                    command = [str(compiler), '--check-project-method-bodies', str(project)]
                    result = subprocess.run(command, cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
                    print('CASE', family, mode, route, 'safe' if safe else 'escape', '\nSOURCE', source,
                          '\nCOMMAND', subprocess.list2cmdline(command), 'EXIT', result.returncode,
                          result.stdout + result.stderr, flush=True)
                    if (result.returncode == 0) != safe:
                        failures.append((family, mode, route, safe, result.returncode))
                    elif not safe and 'view output cannot outlive its possible owner' not in result.stdout + result.stderr:
                        failures.append((family, mode, route, safe, 'wrong diagnostic'))
print('FAILURES', failures, flush=True)
sys.exit(bool(failures))
