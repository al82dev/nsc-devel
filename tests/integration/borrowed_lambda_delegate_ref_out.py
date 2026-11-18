"""Mutable delegate arguments cannot invalidate an active closure-owned view."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def source_for(family, output_mode, pass_kind, route, timing):
    prefix = ('raii Holder owner = new Holder(); SaveAction action = [move owner] (' + output_mode +
              ' SharedView<Payload> output) => { owner.Save(' + output_mode + ' output); }; ')
    setup = ('Reset' + pass_kind.title() + 'Action reset = Reset' + pass_kind.title() + '; '
             if route == 'delegate' else
             'Signal += Reset' + pass_kind.title() + '; ' if route == 'event' else '')
    call = ('Reset' + pass_kind.title() if route == 'method' else
            'Helpers.Reset' + pass_kind.title() if route == 'namespace' else
            'reset' if route == 'delegate' else 'Signal')
    replacement = call + '(' + pass_kind + ' action); '
    view = 'SharedView<Payload> result = default(SharedView<Payload>); '
    invoke = 'action(' + output_mode + ' result); '
    main = (prefix + setup + view + invoke + replacement + 'return result.Get().Data;' if timing == 'active' else
            prefix + setup + '{ ' + view + invoke + 'int data = result.Get().Data; } ' + replacement + 'return 42;')
    return ('using NativeSharp.Core; delegate void SaveAction(' + output_mode +
            ' SharedView<Payload> output); delegate void ResetRefAction(ref SaveAction action); '
            'delegate void ResetOutAction(out SaveAction action); public class Payload { '
            'public int Data; public Payload() { Data = 42; } } public class Holder { private ' +
            family + '<Payload> inner; public Holder() { inner = SharedPointers.Make' + family +
            '<Payload>(); } public void Save(' + output_mode + ' SharedView<Payload> output) '
            '{ output = inner.Borrow(); } } namespace Helpers { public void ResetRef(ref SaveAction action) '
            '{ action = default(SaveAction); } public void ResetOut(out SaveAction action) '
            '{ action = default(SaveAction); } } class Program { public static event Reset' +
            pass_kind.title() + 'Action Signal; static void ResetRef(ref SaveAction action) '
            '{ action = default(SaveAction); } static void ResetOut(out SaveAction action) '
            '{ action = default(SaveAction); } public static int Main() { ' + main + ' } }')


def run(command, root):
    result = subprocess.run([str(value) for value in command], cwd=root, capture_output=True,
                            text=True, errors='replace', timeout=120)
    print('COMMAND', subprocess.list2cmdline([str(value) for value in command]), 'EXIT', result.returncode,
          result.stdout + result.stderr, flush=True)
    return result


def main():
    compiler = Path(sys.argv[1]).resolve()
    core_argument = os.environ.get('NSC_TEST_CORE_BINARY')
    if not core_argument:
        print('SKIP: set NSC_TEST_CORE_BINARY to the isolated Core library')
        return 77
    core = Path(core_argument).resolve()
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-delegate-view-ref-out-') as temporary:
        root = Path(temporary)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup><ItemGroup>'
                           '<Compile Include="Main.ns"/><LibraryReference Include="' + core.as_posix() +
                           '" Metadata="' + core.with_suffix('.nsmeta').as_posix() + '"/></ItemGroup></Project>',
                           encoding='utf-8')

        def check(source, name, expected):
            (root / 'Main.ns').write_text(source, encoding='utf-8')
            result = run([compiler, '--check-project-method-bodies', project], root)
            valid = result.returncode == 0
            if valid != expected or (not expected and 'cannot pass delegate for replacement while '
                                     'a borrowed local is alive' not in result.stdout + result.stderr):
                failures.append((name, 'analysis', result.returncode))

        def execute(source, name):
            (root / 'Main.ns').write_text(source, encoding='utf-8')
            for level in ('O0', 'O2'):
                executable = root / (name + '-' + level + '.exe')
                built = run([compiler, '--emit-project-oop-exe', project, '-' + level, '-o', executable,
                             '--no-artifact-cache', '--no-error-log'], root)
                if built.returncode:
                    failures.append((name, level, 'build'))
                elif run([executable], root).returncode != 42:
                    failures.append((name, level, 'run'))

        for family in ('Rc', 'Arc'):
            for output_mode in ('ref', 'out'):
                for pass_kind in ('ref', 'out'):
                    for route in ('method', 'namespace', 'delegate', 'event'):
                        for timing in ('active', 'after'):
                            name = '-'.join((family, output_mode, pass_kind, route, timing))
                            check(source_for(family, output_mode, pass_kind, route, timing), name, timing == 'after')
        for route in ('method', 'namespace'):
            for pass_kind in ('ref', 'out'):
                for timing in ('active', 'after'):
                    source = source_for('Rc', 'ref', pass_kind, route, timing)
                    source = source.replace('Reset' + pass_kind.title() + '(' + pass_kind + ' action);',
                                            'Reset' + pass_kind.title() + '(action: ' + pass_kind + ' action);')
                    check(source, 'named-' + route + '-' + pass_kind + '-' + timing, timing == 'after')
        for route in ('method', 'namespace', 'delegate', 'event'):
            for pass_kind in ('ref', 'out'):
                active = source_for('Rc', 'ref', pass_kind, route, 'active')
                after = source_for('Rc', 'ref', pass_kind, route, 'after')
                absent = active.replace('action(ref result); ', '').replace('return result.Get().Data;', 'return 42;')
                execute(after, route + '-after-' + pass_kind)
                execute(absent, route + '-absent-' + pass_kind)
            active = source_for('Rc', 'ref', 'ref', route, 'active')
            for pass_kind in ('in', 'value'):
                qualifier = 'in ' if pass_kind == 'in' else ''
                safe = active.replace('ResetRef(ref SaveAction action) { action = default(SaveAction); }',
                                      'ResetRef(' + qualifier + 'SaveAction action) { }')
                safe = safe.replace('ResetRefAction(ref SaveAction action)',
                                    'ResetRefAction(' + qualifier + 'SaveAction action)')
                safe = safe.replace('ref action);', qualifier + 'action);')
                execute(safe, route + '-' + pass_kind)
    print('FAILURES', failures, flush=True)
    return bool(failures)


if __name__ == '__main__':
    sys.exit(main())
