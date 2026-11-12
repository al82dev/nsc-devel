"""A view written by a closure must not outlive its captured owner."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

compiler = Path(sys.argv[1]).resolve()
core_argument = os.environ.get('NSC_TEST_CORE_BINARY')
if not core_argument:
    print('SKIP: set NSC_TEST_CORE_BINARY to the isolated Core library')
    sys.exit(77)
core = Path(core_argument).resolve()
failures = []
with tempfile.TemporaryDirectory(prefix='nsc-view-receiver-output-') as directory:
    root = Path(directory)
    project = root / 'Probe.nsproj'
    project.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup><ItemGroup>'
                       '<Compile Include="Main.ns"/><LibraryReference Include="' + core.as_posix() +
                       '" Metadata="' + core.with_suffix('.nsmeta').as_posix() + '"/></ItemGroup></Project>', encoding='utf-8')
    for family in ('Rc', 'Arc'):
        for mode in ('ref', 'out'):
            for variant in ('escape', 'live', 'replace', 'after', 'ref_replace', 'out_replace',
                            'after_ref', 'after_out', 'other_ref', 'other_out', 'invoke_again', 'invoke_alias'):
                declaration = 'SharedView<Payload> result = default(SharedView<Payload>); '
                holder = 'raii Holder owner = new Holder(); '
                method = 'ChangeAndSave' if variant in ('invoke_again', 'invoke_alias') else 'Save'
                setup = ('SaveAction action = [move owner] (' + mode + ' SharedView<Payload> output) => '
                         '{ owner.' + method + '(' + mode + ' output); }; ')
                invoke = 'action(' + mode + ' result); '
                call = setup + ('SaveAction alias = action; ' if variant == 'invoke_alias' else '') + invoke
                replacement = 'action = default(SaveAction); '
                main = (declaration + '{ ' + holder + call + '} return result.Get().Data;' if variant == 'escape' else
                        holder + setup + '{ ' + declaration + invoke + 'int data = result.Get().Data; } '
                        + (replacement if variant == 'after' else 'ResetRef(ref action); ' if variant == 'after_ref'
                           else 'ResetOut(out action); ') + 'return 42;' if variant in ('after', 'after_ref', 'after_out') else
                        holder + declaration + call +
                        (replacement if variant == 'replace' else 'ResetRef(ref action); ' if variant == 'ref_replace'
                         else 'ResetOut(out action); ' if variant == 'out_replace'
                         else 'SaveAction other = action; ResetRef(ref other); ' if variant == 'other_ref'
                         else 'SaveAction other = action; ResetOut(out other); ' if variant == 'other_out'
                         else 'SharedView<Payload> second = default(SharedView<Payload>); action(' + mode + ' second); '
                         if variant == 'invoke_again'
                         else 'SharedView<Payload> second = default(SharedView<Payload>); alias(' + mode + ' second); '
                         if variant == 'invoke_alias' else '') +
                        'return result.Get().Data;')
                source = ('using NativeSharp.Core; delegate void SaveAction(' + mode + ' SharedView<Payload> output); public class Payload { public int Data; '
                          'public Payload() { Data = 42; } } public class Holder { '
                          'private ' + family + '<Payload> inner; public Holder() { inner = SharedPointers.Make' +
                          family + '<Payload>(); } public void Reset() { inner = SharedPointers.Make' + family +
                          '<Payload>(); } public void Save(' + mode +
                          ' SharedView<Payload> output) { output = inner.Borrow(); } public void ChangeAndSave(' + mode +
                          ' SharedView<Payload> output) { Reset(); Save(' + mode + ' output); } } '
                          'class Program { static void ResetRef(ref SaveAction action) { action = default(SaveAction); } '
                          'static void ResetOut(out SaveAction action) { action = default(SaveAction); } '
                          'public static int Main() { ' + main + ' } }')
                (root / 'Main.ns').write_text(source, encoding='utf-8')
                command = [str(compiler), '--check-project-method-bodies', str(project)]
                result = subprocess.run(command, cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
                print('CASE', family, mode, variant, '\nSOURCE', source,
                      '\nCOMMAND', subprocess.list2cmdline(command), 'EXIT', result.returncode,
                      result.stdout + result.stderr, flush=True)
                if (result.returncode == 0) != (variant in ('live', 'after', 'after_ref', 'after_out',
                                                           'other_ref', 'other_out')):
                    failures.append((family, mode, variant, result.returncode))
                elif variant in ('invoke_again', 'invoke_alias') and 'FnOnce closure local' in result.stdout + result.stderr:
                    pass
                elif result.returncode and not any(message in result.stdout + result.stderr for message in
                    ('view output cannot outlive its possible owner', 'while a borrowed local is alive',
                     'cannot call potentially mutating method while a borrowed field view is alive')):
                    failures.append((family, mode, variant, 'wrong diagnostic'))
                if variant in ('live', 'after', 'after_ref', 'after_out', 'other_ref', 'other_out') and result.returncode == 0 and os.environ.get('NSC_VIEW_TEST_EXECUTE_VALID') == '1':
                    for level in ('O0', 'O2'):
                        executable = root / (family + '-' + mode + '-' + variant + '-' + level + '.exe')
                        build = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level,
                                 '-o', str(executable), '--no-artifact-cache']
                        built = subprocess.run(build, cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
                        print('BUILD', family, mode, level, 'EXIT', built.returncode, built.stdout + built.stderr, flush=True)
                        if built.returncode:
                            failures.append((family, mode, level, 'build'))
                            continue
                        ran = subprocess.run([str(executable)], cwd=root, capture_output=True, text=True,
                                             errors='replace', timeout=30)
                        print('RUN', family, mode, level, 'EXIT', ran.returncode, ran.stdout + ran.stderr, flush=True)
                        if ran.returncode != 42:
                            failures.append((family, mode, level, 'execution', ran.returncode))
print('FAILURES', failures, flush=True)
sys.exit(bool(failures))
