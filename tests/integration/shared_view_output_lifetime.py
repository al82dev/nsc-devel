"""A direct view output must not borrow an owner destroyed on return."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

compiler = Path(sys.argv[1]).resolve()
core = Path(sys.argv[2]).resolve() / 'lib' / 'NativeSharp.Core'
sources = ('SharedStorage', 'SharedControlStorage', 'SharedReferenceCounts', 'SharedLocalControl',
           'SharedAtomicControl', 'SharedBorrow', 'SharedLocalHandle', 'SharedLocalWeak',
           'SharedLocalFactory', 'SharedAtomicHandle', 'SharedAtomicWeak', 'SharedAtomicFactory',
           'SharedView', 'Rc', 'WeakRc', 'Arc', 'WeakArc', 'SharedPointers')
failures = []
with tempfile.TemporaryDirectory(prefix='nsc-shared-view-output-') as directory:
    root = Path(directory)
    for name in sources:
        shutil.copyfile(core / (name + '.ns'), root / (name + '.ns'))
    project = root / 'Probe.nsproj'
    project.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup>'
                       '<ItemGroup><Compile Include="*.ns"/></ItemGroup></Project>', encoding='utf-8')
    for family in ('Rc', 'Arc'):
        for mode in ('ref', 'out'):
            for variant in ('local', 'consumed', 'live'):
                parameters = ('SharedView<Payload> input, ' if variant == 'live' else
                              family + '<Payload> input, ' if variant == 'consumed' else '')
                owner = (family + '<Payload> local = SharedPointers.Make' + family + '<Payload>(); '
                         if variant == 'local' else '')
                view = 'input' if variant == 'live' else 'input.Borrow()' if variant == 'consumed' else 'local.Borrow()'
                caller_owner = family + '<Payload> owner = SharedPointers.Make' + family + '<Payload>(); '
                argument = 'owner.Borrow(), ' if variant == 'live' else 'move(owner), ' if variant == 'consumed' else ''
                source = ('using NativeSharp.Core; namespace NativeSharp.Core { public class Exception {} '
                          'public trait Send {} public trait Sync {} } '
                          'class Payload { public int Data; public Payload() { Data = 42; } } '
                          'class Sink { public static void Store(' + parameters + mode +
                          ' SharedView<Payload> output) { ' + owner + 'output = ' + view + '; } } '
                          'class Program { public static int Main() { ' + caller_owner +
                          'SharedView<Payload> result = default(SharedView<Payload>); Sink.Store(' + argument +
                          mode + ' result); return result.Get().Data; } }')
                (root / 'Main.ns').write_text(source, encoding='utf-8')
                command = [str(compiler), '--check-project-method-bodies', str(project)]
                checked = subprocess.run(command, cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
                print('CASE', family, mode, variant, '\nSOURCE', source, '\nCOMMAND',
                      subprocess.list2cmdline(command), 'EXIT', checked.returncode,
                      checked.stdout + checked.stderr, flush=True)
                if variant != 'live':
                    if checked.returncode == 0 or 'view output cannot outlive its possible owner' not in checked.stdout + checked.stderr:
                        failures.append((family, mode, variant, 'diagnostic'))
                    continue
                if checked.returncode:
                    failures.append((family, mode, variant, 'check'))
                    continue
                for level in ('O0', 'O2'):
                    executable = root / (family + '-' + mode + '-' + level + '.exe')
                    command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level, '-o', str(executable),
                               '--no-artifact-cache']
                    built = subprocess.run(command, cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
                    print('BUILD', family, mode, level, 'EXIT', built.returncode, built.stdout + built.stderr, flush=True)
                    if built.returncode:
                        failures.append((family, mode, level, 'build'))
                        continue
                    ran = subprocess.run([str(executable)], cwd=root, capture_output=True, text=True, errors='replace', timeout=30)
                    print('RUN', family, mode, level, 'EXIT', ran.returncode, ran.stdout + ran.stderr, flush=True)
                    if ran.returncode != 42:
                        failures.append((family, mode, level, 'execution', ran.returncode))
print('FAILURES', failures, flush=True)
sys.exit(bool(failures))
