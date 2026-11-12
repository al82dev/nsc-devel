"""Repeated reads of one move capture must not move its source repeatedly."""
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


def source_for(family, case):
    field = '' if family == 'plain' else 'private ' + family + '<Payload> inner; '
    init = '' if family == 'plain' else 'inner = SharedPointers.Make' + family + '<Payload>(); '
    twice = 'int first = owner.Read(); int second = owner.Read(); return first + second;'
    once = 'int first = owner.Read(); return first * 2;'
    body = (once if case in ('once', 'no_call', 'explicit_delete', 'after_move', 'two_closures') else
            'raii Holder next = move(owner); return owner.Read();' if case == 'move_inside' else twice)
    if case == 'nested_direct':
        body = ('int first = owner.Read(); ReadAction inner = [move owner] () => '
                '{ return owner.Read(); }; return first + inner();')
    elif case == 'nested_only':
        body = ('ReadAction inner = [move owner] () => { int first = owner.Read(); '
                'int second = owner.Read(); return first + second; }; return inner();')
    elif case == 'nested_bad':
        body = ('ReadAction inner = [move owner] () => { return owner.Read(); }; '
                'int first = inner(); return first + owner.Read();')
    action_type = 'ReadAction' if case == 'explicit_delete' else 'raii ReadAction'
    action = action_type + ' action = [move owner] () => { ' + body + ' }; '
    extra = ('int bad = owner.Read(); ' if case == 'after_move' else
             'ReadAction other = [move owner] () => { return owner.Read(); }; ' if case == 'two_closures' else '')
    method = ('static int From(owned Holder owner) { ' + action + 'return action(); } '
              if case == 'parameter_twice' else '')
    entry = ('value = From(move(owner));' if case == 'parameter_twice' else
             action + extra + ('value = 42;' if case == 'no_call' else 'value = action();') +
             (' delete action;' if case == 'explicit_delete' else ''))
    return ('using NativeSharp.Core; delegate int ReadAction(); class State { public static int Drops; } '
            'public class Payload { public Payload() { } } public class Holder { ' + field +
            'public Holder() { ' + init + '} ~Holder() { State.Drops = State.Drops + 1; } '
            'public int Read() { return 21; } } class Program { ' + method +
            'public static int Main() { State.Drops = 0; int value = 0; { raii Holder owner = new Holder(); ' +
            entry + ' } return value == 42 && State.Drops == 1 ? 42 : 1; } }')


def main():
    compiler = Path(sys.argv[1]).resolve()
    core_argument = os.environ.get('NSC_TEST_CORE_BINARY')
    if not core_argument:
        print('SKIP: set NSC_TEST_CORE_BINARY to the isolated Core library')
        return 77
    core = Path(core_argument).resolve()
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-lambda-move-reuse-') as temporary:
        root = Path(temporary)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup><ItemGroup>'
                           '<Compile Include="Main.ns"/><LibraryReference Include="' + core.as_posix() +
                           '" Metadata="' + core.with_suffix('.nsmeta').as_posix() + '"/></ItemGroup></Project>',
                           encoding='utf-8')
        valid = ('once', 'twice', 'parameter_twice', 'no_call', 'explicit_delete', 'nested_direct', 'nested_only')
        invalid = ('after_move', 'two_closures', 'move_inside', 'nested_bad')
        for family in ('plain', 'Rc', 'Arc'):
            for case in valid + invalid:
                source = source_for(family, case)
                (root / 'Main.ns').write_text(source, encoding='utf-8')
                result = run([compiler, '--check-project-method-bodies', project], root)
                if (result.returncode == 0) != (case in valid):
                    failures.append((family, case, 'analysis', result.returncode))
                    continue
                if case in invalid and 'moved' not in result.stdout + result.stderr:
                    failures.append((family, case, 'diagnostic'))
                # Nested cases currently verify semantic binding only: their owned
                # environment destruction requires its own dedicated coverage.
                if case not in valid or case.startswith('nested_'):
                    continue
                for level in ('O0', 'O2'):
                    exe = root / (family + '-' + case + '-' + level + '.exe')
                    build = run([compiler, '--emit-project-oop-exe', project, '-' + level, '-o', exe,
                                 '--no-artifact-cache', '--no-error-log'], root)
                    if build.returncode:
                        failures.append((family, case, level, 'build'))
                    elif run([exe], root).returncode != 42:
                        failures.append((family, case, level, 'execution'))
    print('FAILURES', failures, flush=True)
    return bool(failures)


if __name__ == '__main__':
    sys.exit(main())
