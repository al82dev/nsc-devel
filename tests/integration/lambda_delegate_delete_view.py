"""Deleting a delegate may not invalidate a view of its moved capture."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def run(command, root):
    values = [str(value) for value in command]
    result = subprocess.run(values, cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
    print('COMMAND', subprocess.list2cmdline(values), 'EXIT', result.returncode,
          result.stdout + result.stderr, flush=True)
    return result


def source_for(family, mode, variant):
    owned_delegate = variant == 'raii_safe'
    owner = ('raii Holder owner = new Holder(); ' + ('raii SaveAction' if owned_delegate else 'SaveAction') +
             ' action = [move owner] (' + mode + ' SharedView<Payload> output) => '
             '{ owner.Save(' + mode + ' output); }; ')
    view = 'SharedView<Payload> result = default(SharedView<Payload>); '
    invoke = 'action(' + mode + ' result); '
    if variant == 'raii_safe':
        body = ('State.Drops = 0; int observed = 0; { ' + owner + '{ ' + view + invoke +
                'observed = result.Get().Data; } } return observed == 42 && State.Drops == 1 ? 42 : 1;')
    elif variant == 'after':
        body = ('State.Drops = 0; int observed = 0; { ' + owner + '{ ' + view + invoke +
                'observed = result.Get().Data; } delete action; } '
                'return observed == 42 && State.Drops == 1 ? 42 : 1;')
    elif variant == 'other':
        body = ('State.Drops = 0; int observed = 0; { ' + owner + '{ ' + view + invoke +
                'SaveAction other = (' + mode + ' SharedView<Payload> output) => '
                '{ output = default(SharedView<Payload>); }; delete other; '
                'observed = result.Get().Data; } delete action; } '
                'return observed == 42 && State.Drops == 1 ? 42 : 1;')
    elif variant == 'alias':
        body = owner + 'SaveAction alias = action; ' + view + invoke + 'delete alias; return result.Get().Data;'
    elif variant == 'reverse':
        body = (owner + 'SaveAction alias = action; ' + view + 'alias(' + mode +
                ' result); delete action; return result.Get().Data;')
    elif variant == 'chain':
        body = (owner + 'SaveAction first = action; SaveAction alias = first; ' + view + invoke +
                'delete alias; return result.Get().Data;')
    elif variant == 'reassigned':
        body = (owner + 'SaveAction alias = default(SaveAction); alias = action; ' + view + invoke +
                'delete alias; return result.Get().Data;')
    elif variant == 'conditional':
        body = (owner + 'SaveAction other = default(SaveAction); bool choose = true; '
                'SaveAction alias = choose ? action : other; ' + view + invoke +
                'delete alias; return result.Get().Data;')
    else:
        body = owner + view + invoke + 'delete action; return result.Get().Data;'
    return ('using NativeSharp.Core; delegate void SaveAction(' + mode + ' SharedView<Payload> output); '
            'class State { public static int Drops; } public class Payload { public int Data; '
            'public Payload() { Data = 42; } } public class Holder { private ' + family +
            '<Payload> inner; public Holder() { inner = SharedPointers.Make' + family +
            '<Payload>(); } ~Holder() { State.Drops = State.Drops + 1; } public void Save(' + mode +
            ' SharedView<Payload> output) { output = inner.Borrow(); } } class Program { '
            'public static int Main() { ' + body + ' } }')


def main():
    compiler = Path(sys.argv[1]).resolve()
    core_argument = os.environ.get('NSC_TEST_CORE_BINARY')
    if not core_argument:
        print('SKIP: set NSC_TEST_CORE_BINARY to the isolated Core library')
        return 77
    core = Path(core_argument).resolve()
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-delegate-delete-view-') as temporary:
        root = Path(temporary)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup><ItemGroup>'
                           '<Compile Include="Main.ns"/><LibraryReference Include="' + core.as_posix() +
                           '" Metadata="' + core.with_suffix('.nsmeta').as_posix() + '"/></ItemGroup></Project>',
                           encoding='utf-8')
        valid = ('raii_safe', 'after', 'other')
        invalid = ('direct', 'alias', 'reverse', 'chain', 'reassigned', 'conditional')
        for family in ('Rc', 'Arc'):
            for mode in ('ref', 'out'):
                for variant in valid + invalid:
                    (root / 'Main.ns').write_text(source_for(family, mode, variant), encoding='utf-8')
                    result = run([compiler, '--check-project-method-bodies', project], root)
                    if (result.returncode == 0) != (variant in valid):
                        failures.append((family, mode, variant, 'analysis', result.returncode))
                        continue
                    if variant in invalid:
                        if 'cannot delete delegate while a borrowed local is alive' not in result.stdout + result.stderr:
                            failures.append((family, mode, variant, 'diagnostic'))
                        continue
                    for level in ('O0', 'O2'):
                        exe = root / (family + '-' + mode + '-' + variant + '-' + level + '.exe')
                        build = run([compiler, '--emit-project-oop-exe', project, '-' + level, '-o', exe,
                                     '--no-artifact-cache', '--no-error-log'], root)
                        if build.returncode:
                            failures.append((family, mode, variant, level, 'build'))
                        elif run([exe], root).returncode != 42:
                            failures.append((family, mode, variant, level, 'execution'))
    print('FAILURES', failures, flush=True)
    return bool(failures)


if __name__ == '__main__':
    sys.exit(main())
