"""Prevent aliases hidden inside readonly struct results from escaping."""

from html import escape
from pathlib import Path
import subprocess
import sys

from isolated_a_runner import absolute_a, setup


PRODUCER = '''public class Payload { public int Data; public Payload() { Data = 42; } public void Mutate() { Data = 7; } }
public struct Pair { public Payload child; }
public struct Scalar { public int Data; }
public struct Hidden { private Payload secret; public Hidden() { secret = new Payload(); }
 public void Mutate() { secret.Data = 7; } }
public struct Nested { public Hidden hidden; }
public struct RefHolder { public ref int Field; }
public struct Owner { public Pair pair; public Scalar scalar; public Hidden hidden; public Nested nested; public RefHolder refHolder;
 public Owner() { pair.child = new Payload(); scalar.Data = 42; hidden = new Hidden(); nested.hidden = new Hidden(); } }
public class Access { public static Pair Global;
 public static ref readonly Pair Pick(ref Owner owner) { return ref owner.pair; }
 public static ref readonly[static] Pair StaticPick() { return ref Global; }
 public ref readonly[static] Pair InstancePick() { return ref Global; }
 public static ref readonly Scalar ScalarPick(ref Owner owner) { return ref owner.scalar; }
 public static ref readonly Hidden HiddenPick(ref Owner owner) { return ref owner.hidden; }
 public static ref readonly Nested NestedPick(ref Owner owner) { return ref owner.nested; }
 public static ref readonly RefHolder RefPick(ref Owner owner) { return ref owner.refHolder; }
 public static ref Pair Mutable(ref Owner owner) { return ref owner.pair; }
}
'''
CASES = {
    'alias': ('Owner owner = new Owner(); Pair alias = Access.Pick(ref owner); alias.child.Data = 7; return 42;', False),
    'direct': ('Owner owner = new Owner(); Access.Pick(ref owner).child.Data = 7; return 42;', False),
    'call': ('Owner owner = new Owner(); Access.Pick(ref owner).child.Mutate(); return 42;', False),
    'static_alias': ('Access.Global.child = new Payload(); Pair alias = Access.StaticPick(); alias.child.Data = 7; return 42;', False),
    'instance_alias': ('Access.Global.child = new Payload(); Access access = new Access(); Pair alias = access.InstancePick(); alias.child.Data = 7; return 42;', False),
    'hidden_alias': ('Owner owner = new Owner(); Hidden value = Access.HiddenPick(ref owner); value.Mutate(); return 42;', False),
    'nested_hidden_alias': ('Owner owner = new Owner(); Nested value = Access.NestedPick(ref owner); value.hidden.Mutate(); return 42;', False),
    'ref_field_alias': ('Owner owner = new Owner(); RefHolder value = Access.RefPick(ref owner); value.Field = 7; return 42;', False),
    'read': ('Owner owner = new Owner(); return Access.Pick(ref owner).child.Data;', True),
    'static_read': ('Access.Global.child = new Payload(); return Access.StaticPick().child.Data;', True),
    'instance_read': ('Access.Global.child = new Payload(); Access access = new Access(); return access.InstancePick().child.Data;', True),
    'mutable': ('Owner owner = new Owner(); Access.Mutable(ref owner).child.Data = 42; return owner.pair.child.Data;', True),
    'scalar_copy': ('Owner owner = new Owner(); Scalar value = Access.ScalarPick(ref owner); return value.Data;', True),
}


def invoke(command, repo, environment):
    print('COMMAND', subprocess.list2cmdline([str(value) for value in command]), flush=True)
    return subprocess.run([str(value) for value in command], cwd=repo, env=environment,
                          capture_output=True, text=True, errors='replace', timeout=120)


def main():
    if len(sys.argv) != 3:
        raise SystemExit('expected nsc and a new evidence directory on A:')
    compiler, root = (absolute_a(value) for value in sys.argv[1:])
    repo = Path(__file__).resolve().parents[2]
    environment = setup(root, repo)
    (root / 'Producer.ns').write_text(PRODUCER, encoding='utf-8')
    producer = root / 'Producer.nsproj'
    producer.write_text('<Project><PropertyGroup><Name>Producer</Name><OutputType>StaticLibrary</OutputType>'
                        '</PropertyGroup><ItemGroup><Compile Include="Producer.ns"/></ItemGroup></Project>',
                        encoding='utf-8')
    failures = []
    for producer_level in ('source', 'O0', 'O2'):
        library = root / ('Producer-' + producer_level + '.lib')
        if producer_level != 'source':
            built = invoke([compiler, '--emit-project-oop-lib', producer, '-' + producer_level,
                            '-o', library, '--no-artifact-cache'], repo, environment)
            if built.returncode:
                failures.append((producer_level, 'producer', built.stdout + built.stderr))
                continue
        for name, (body, valid) in CASES.items():
            case = root / (producer_level + '-' + name)
            case.mkdir()
            program = 'class Program { public static int Main() { ' + body + ' } }\n'
            (case / 'Main.ns').write_text((PRODUCER if producer_level == 'source' else '') + program, encoding='utf-8')
            reference = '' if producer_level == 'source' else (
                '<LibraryReference Include="' + escape(library.as_posix(), quote=True) +
                '" Metadata="' + escape(library.with_suffix('.nsmeta').as_posix(), quote=True) + '"/>')
            project = case / 'Probe.nsproj'
            project.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup><ItemGroup>'
                               '<Compile Include="Main.ns"/>' + reference + '</ItemGroup></Project>', encoding='utf-8')
            for level in ('O0', 'O2'):
                executable = case / (level + '.exe')
                command = [compiler, '--check-project-method-bodies', project] if not valid else [
                    compiler, '--emit-project-oop-exe', project, '-' + level, '-o', executable, '--no-artifact-cache']
                built = invoke(command, repo, environment)
                if not valid:
                    output = built.stdout + built.stderr
                    if built.returncode == 0 or not any(term in output.lower() for term in ('readonly', 'borrow', 'assignable')):
                        failures.append((producer_level, name, level, 'expected readonly rejection', output))
                    else:
                        print('REJECT', producer_level, name, level, flush=True)
                    continue
                if built.returncode:
                    failures.append((producer_level, name, level, 'compile', built.stdout + built.stderr))
                    continue
                result = invoke([executable], repo, environment)
                print('RUN', producer_level, name, level, result.returncode, flush=True)
                if result.returncode != 42:
                    failures.append((producer_level, name, level, 'run', result.returncode))
    for failure in failures:
        print('FAIL', *failure, file=sys.stderr)
    return int(bool(failures))


if __name__ == '__main__':
    sys.exit(main())
