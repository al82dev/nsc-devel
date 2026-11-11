"""Shared owners can lend a view through an in receiver without becoming mutable."""
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
with tempfile.TemporaryDirectory(prefix='nsc-readonly-shared-borrow-') as directory:
    root = Path(directory)
    for name in sources:
        shutil.copyfile(core / (name + '.ns'), root / (name + '.ns'))
    project = root / 'Probe.nsproj'
    project.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup>'
                       '<ItemGroup><Compile Include="*.ns"/></ItemGroup></Project>', encoding='utf-8')
    for family in ('Rc', 'Arc'):
        prefix = ('using NativeSharp.Core; namespace NativeSharp.Core { public class Exception {} '
                  'public trait Send {} public trait Sync {} } '
                  'class Payload { public int Data; public Payload() { Data = 42; } } ')
        valid = (prefix + 'class Program { static int Observe(in ' + family +
                 '<Payload> input) { SharedView<Payload> view = input.Borrow(); return view.Get().Data; } '
                 'public static int Main() { ' + family + '<Payload> owner = SharedPointers.Make' + family +
                 '<Payload>(); return Observe(in owner); } }')
        (root / 'Main.ns').write_text(valid, encoding='utf-8')
        for level in ('O0', 'O2'):
            executable = root / (family + '-' + level + '.exe')
            command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level, '-o', str(executable),
                       '--no-artifact-cache']
            built = subprocess.run(command, cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
            print('BUILD', family, level, subprocess.list2cmdline(command), 'EXIT', built.returncode,
                  built.stdout + built.stderr, flush=True)
            if built.returncode:
                failures.append((family, level, 'build'))
                continue
            ran = subprocess.run([str(executable)], cwd=root, capture_output=True, text=True, errors='replace', timeout=30)
            print('RUN', family, level, 'EXIT', ran.returncode, ran.stdout + ran.stderr, flush=True)
            if ran.returncode != 42:
                failures.append((family, level, 'execution', ran.returncode))
        invalid = (prefix + 'class Program { static void Mutate(in ' + family +
                   '<Payload> input) { input.Clone(); } public static int Main() { return 0; } }')
        (root / 'Main.ns').write_text(invalid, encoding='utf-8')
        result = subprocess.run([str(compiler), '--check-project-method-bodies', str(project)], cwd=root,
                                capture_output=True, text=True, errors='replace', timeout=120)
        print('READONLY', family, 'EXIT', result.returncode, result.stdout + result.stderr, flush=True)
        if result.returncode == 0 or "readonly receiver cannot invoke mutating struct member 'Clone'" not in result.stdout + result.stderr:
            failures.append((family, 'readonly'))
print('FAILURES', failures, flush=True)
sys.exit(bool(failures))
