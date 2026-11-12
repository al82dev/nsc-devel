"""A tuple view copied to a ref output must not outlive its shared owner."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

core_argument = sys.argv[2] if len(sys.argv) > 2 else os.environ.get('NSC_TEST_CORE_BINARY')
if not core_argument:
    print('SKIP: set NSC_TEST_CORE_BINARY to an isolated NativeSharp.Core library', flush=True)
    sys.exit(77)
compiler, core = (Path(value).resolve() for value in (sys.argv[1], core_argument))
repo = Path(__file__).resolve().parents[2]
failures = []
with tempfile.TemporaryDirectory(prefix='nsc-view-ref-output-') as directory:
    root = Path(directory)
    project = root / 'Probe.nsproj'
    project.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup><ItemGroup>'
        '<Compile Include="Main.ns"/><LibraryReference Include="' + core.as_posix() +
        '" Metadata="' + core.with_suffix('.nsmeta').as_posix() + '"/></ItemGroup></Project>', encoding='utf-8')
    for family in ('Rc', 'Arc'):
        for variant in ('direct', 'forward', 'live', 'replace', 'after',
                        'scalar', 'scalar_live', 'scalar_replace', 'scalar_alias', 'tuple_alias', 'static',
                        'out', 'out_live', 'out_replace', 'out_scalar', 'out_scalar_live', 'staticfield',
                        'ns', 'ns_live', 'ns_replace', 'ns_scalar', 'ns_out',
                        'delegate', 'delegate_live', 'delegate_replace', 'delegate_scalar', 'delegate_out',
                        'event', 'event_live', 'event_replace', 'event_scalar', 'event_out',
                        'identity', 'identity_live', 'identity_replace', 'identity_tuple',
                        'callback', 'callback_live', 'callback_replace', 'callback_tuple',
                        'tupleout', 'tupleout_live', 'tupleout_replace', 'tupleout_out',
                        'tupleout_alias', 'tupleout_alias_live'):
            store = ('public static void Store(ref SharedView<Payload> output, SharedView<Payload> view) '
                     '{ output = view; } ' if variant == 'forward' else '')
            tuple_output = variant.startswith('tupleout')
            action = 'Store(ref output, view);' if variant == 'forward' else 'output = (view, 0);' if tuple_output else 'output = view;'
            declaration = ('(SharedView<Payload>, int) result = (default(SharedView<Payload>), 0); ' if tuple_output else
                           ('static ' if variant == 'static' else '') + 'SharedView<Payload> result = default(SharedView<Payload>); ')
            owner = family + '<Payload> owner = SharedPointers.Make' + family + '<Payload>(); '
            scalar = tuple_output or variant.startswith('scalar') or variant.startswith('out_scalar') or variant in ('ns_scalar', 'delegate_scalar', 'event_scalar', 'identity', 'identity_live', 'identity_replace', 'callback', 'callback_live', 'callback_replace')
            namespace = variant.startswith('ns')
            delegate = variant.startswith('delegate')
            event = variant.startswith('event')
            pass_kind = 'out' if variant.startswith('out') or variant in ('ns_out', 'delegate_out', 'event_out', 'tupleout_out', 'tupleout_alias', 'tupleout_alias_live') else 'ref'
            alias = variant.endswith('_alias')
            callback = variant.startswith('callback')
            view = ('savedView' if alias else 'id(owner.Borrow())' if callback else 'Sink.Identity(owner.Borrow())' if variant.startswith('identity') else 'owner.Borrow()')
            prepare = ('SharedView<Payload> savedView = owner.Borrow(); ' if alias else
                       'ViewIdentity id = Sink.Identity; ' if callback else '')
            destination = 'Sink.saved' if variant == 'staticfield' else 'result'
            call = (prepare + ('StoreDelegate fn = Sink.Save; ' if delegate else 'Signal += Sink.Save; ' if event else '') +
                    ('fn(' if delegate else 'Signal(' if event else 'Save(' if namespace else 'Sink.Save(') +
                    (view if scalar else '(' + view + ', 0)') + ', ' + pass_kind + ' ' + destination + '); ')
            replace = 'owner = SharedPointers.Make' + family + '<Payload>(); '
            read = 'var (got, number) = result; return got.Get().Data;' if tuple_output else 'return result.Get().Data;'
            if variant in ('live', 'scalar_live', 'out_live', 'out_scalar_live', 'ns_live', 'delegate_live', 'event_live', 'identity_live', 'callback_live', 'tupleout_live', 'tupleout_alias_live'):
                main = '{ ' + owner + declaration + call + read + ' }'
            elif variant in ('replace', 'scalar_replace', 'out_replace', 'ns_replace', 'delegate_replace', 'event_replace', 'identity_replace', 'callback_replace', 'tupleout_replace'):
                main = '{ ' + owner + declaration + call + replace + read + ' }'
            elif variant == 'after':
                main = ('{ ' + owner + '{ ' + declaration + call + 'int value = result.Get().Data; } ' +
                        replace + 'return 42; }')
            elif variant == 'static':
                main = '{ ' + owner + declaration + call + 'return 42; }'
            elif variant == 'staticfield':
                main = '{ ' + owner + call + 'return 42; }'
            else:
                main = declaration + '{ ' + owner + call + '} ' + read
            source = ('using NativeSharp.Core; ' + ('using Fixture; ' if namespace else '') +
                ('using ViewTuple = (SharedView<Payload>, int); ' if variant.startswith('tupleout_alias') else '') +
                ('public delegate void StoreDelegate(' + ('SharedView<Payload>' if scalar else '(SharedView<Payload>, int)') +
                 ' value, ' + pass_kind + ' SharedView<Payload> output); ' if delegate or event else '') +
                ('public delegate SharedView<Payload> ViewIdentity(SharedView<Payload> input); ' if callback else '') +
                'public class Payload { public int Data; '
                'public Payload() { Data = 42; } } ' + ('namespace Fixture { ' if namespace else 'public class Sink { ') +
                ('public static SharedView<Payload> saved; ' if variant == 'staticfield' else '') + store +
                ('public static SharedView<Payload> Identity(SharedView<Payload> input) { return input; } ' if variant.startswith('identity') or callback else '') +
                ('public ' + ('' if namespace else 'static ') + 'void Save(SharedView<Payload> view, ' + pass_kind + (' ViewTuple' if variant.startswith('tupleout_alias') else ' (SharedView<Payload>, int)' if tuple_output else ' SharedView<Payload>') + ' output) {' if scalar else
                 'public ' + ('' if namespace else 'static ') + 'void Save((SharedView<Payload>, int) value, ' + pass_kind + ' SharedView<Payload> output) '
                 '{ var (view, n) = value; ') + action + ' } } class Program { ' +
                ('public static event StoreDelegate Signal; ' if event else '') + 'public static int Main() { '
                + main + ' } }')
            (root / 'Main.ns').write_text(source, encoding='utf-8')
            command = [str(compiler), '--check-project-method-bodies', str(project)]
            result = subprocess.run(command, cwd=repo, capture_output=True, text=True, errors='replace', timeout=120)
            print('CASE', family, variant, '\nSOURCE', source, '\nCOMMAND', subprocess.list2cmdline(command),
                  'EXIT', result.returncode, result.stdout + result.stderr, flush=True)
            if tuple_output:
                limitation = ('expected parameter name' if variant == 'tupleout_out' else
                              'tuple containing a borrowed view cannot be stored outside a tracked local')
                if result.returncode == 0 or limitation not in result.stdout + result.stderr:
                    failures.append((family, variant, 'tuple output restriction changed'))
                else:
                    print('KNOWN LIMITATION', family, variant,
                          'tuple view output rejected before its lifetime can be assessed', flush=True)
            elif (result.returncode == 0) != (variant in ('live', 'after', 'scalar_live', 'out_live', 'out_scalar_live', 'ns_live', 'delegate_live', 'event_live', 'identity_live', 'callback_live')):
                failures.append((family, variant, result.returncode))
            elif result.returncode != 0 and variant == 'staticfield' and "cannot have view type" not in result.stdout + result.stderr:
                failures.append((family, variant, 'unexpected field diagnostic'))
            elif result.returncode != 0 and variant != 'staticfield' and not any(
                    message in result.stdout + result.stderr for message in
                    ('view output cannot outlive its possible owner', 'while a borrowed local is alive')):
                failures.append((family, variant, 'unexpected diagnostic'))
            if (os.environ.get('NSC_VIEW_TEST_EXECUTE_VALID') == '1' and result.returncode == 0 and
                    variant in ('live', 'out_live', 'identity_live', 'ns_live', 'delegate_live', 'event_live', 'callback_live')):
                for level in ('O0', 'O2'):
                    executable = root / (family + '-' + variant + '-' + level + '.exe')
                    build = [str(compiler), '--emit-project-oop-exe', str(project),
                             '-' + level, '-o', str(executable), '--no-artifact-cache']
                    built = subprocess.run(build, cwd=repo, capture_output=True, text=True, errors='replace', timeout=120)
                    print('BUILD', family, variant, level, subprocess.list2cmdline(build),
                          'EXIT', built.returncode, built.stdout + built.stderr, flush=True)
                    if built.returncode:
                        failures.append((family, variant, level, 'build'))
                        continue
                    ran = subprocess.run([str(executable)], cwd=repo, capture_output=True, text=True,
                                         errors='replace', timeout=30)
                    print('RUN', family, variant, level, 'EXIT', ran.returncode,
                          ran.stdout + ran.stderr, flush=True)
                    if ran.returncode != 42:
                        failures.append((family, variant, level, 'execution', ran.returncode))
print('FAILURES', failures, flush=True)
sys.exit(bool(failures))
