"""An output view may borrow an owner passed by reference to the call."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

compiler = Path(sys.argv[1]).resolve()
core = Path(os.environ['NSC_TEST_CORE_BINARY']).resolve()
failures = []
with tempfile.TemporaryDirectory(prefix='nsc-view-owner-ref-') as directory:
    root = Path(directory)
    project = root / 'Probe.nsproj'
    project.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup><ItemGroup>'
                       '<Compile Include="Main.ns"/><LibraryReference Include="' + core.as_posix() +
                       '" Metadata="' + core.with_suffix('.nsmeta').as_posix() + '"/></ItemGroup></Project>', encoding='utf-8')
    for family in ('Rc', 'Arc'):
        for mode in ('ref', 'out'):
            for route in ('direct', 'forward'):
                for variant in ('escape', 'replace', 'live'):
                    body = ('output = input;' if route == 'direct' else
                            'Sink.Save((input, 0), ' + mode + ' output);')
                    declaration = 'SharedView<Payload> result = default(SharedView<Payload>); '
                    owner = family + '<Payload> owner = SharedPointers.Make' + family + '<Payload>(); '
                    call = 'SharedView<Payload> source = owner.Borrow(); Sink.Forward(in source, ' + mode + ' result); '
                    replacement = 'owner = SharedPointers.Make' + family + '<Payload>(); '
                    main = (declaration + '{ ' + owner + call + '} return result.Get().Data;' if variant == 'escape' else
                            owner + declaration + call + (replacement if variant == 'replace' else '') +
                            'return result.Get().Data;')
                    source = ('using NativeSharp.Core; public class Payload { public int Data; '
                              'public Payload() { Data = 42; } } public class Sink { '
                              'public static void Save((SharedView<Payload>, int) pair, ' + mode +
                              ' SharedView<Payload> output) { var (view, number) = pair; output = view; } '
                              'public static void Forward(in SharedView<Payload> input, ' + mode +
                              ' SharedView<Payload> output) { ' + body + ' } } '
                              'class Program { public static int Main() { ' + main + ' } }')
                    (root / 'Main.ns').write_text(source, encoding='utf-8')
                    command = [str(compiler), '--check-project-method-bodies', str(project)]
                    result = subprocess.run(command, cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
                    print('CASE', family, mode, route, variant, '\nSOURCE', source,
                          '\nCOMMAND', subprocess.list2cmdline(command), 'EXIT', result.returncode,
                          result.stdout + result.stderr, flush=True)
                    if (result.returncode == 0) != (variant == 'live'):
                        failures.append((family, mode, route, variant, result.returncode))
                    elif result.returncode and not any(message in result.stdout + result.stderr for message in
                        ('view output cannot outlive its possible owner', 'while a borrowed local is alive')):
                        failures.append((family, mode, route, variant, 'wrong diagnostic'))
                    if variant == 'live' and result.returncode == 0 and os.environ.get('NSC_VIEW_TEST_EXECUTE_VALID') == '1':
                        for level in ('O0', 'O2'):
                            executable = root / (family + '-' + mode + '-' + route + '-' + level + '.exe')
                            build = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level,
                                     '-o', str(executable), '--no-artifact-cache']
                            built = subprocess.run(build, cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
                            print('BUILD', family, mode, route, level, 'EXIT', built.returncode,
                                  built.stdout + built.stderr, flush=True)
                            if built.returncode:
                                failures.append((family, mode, route, level, 'build'))
                                continue
                            ran = subprocess.run([str(executable)], cwd=root, capture_output=True, text=True,
                                                 errors='replace', timeout=30)
                            print('RUN', family, mode, route, level, 'EXIT', ran.returncode,
                                  ran.stdout + ran.stderr, flush=True)
                            if ran.returncode != 42:
                                failures.append((family, mode, route, level, 'execution', ran.returncode))
print('FAILURES', failures, flush=True)
sys.exit(bool(failures))
