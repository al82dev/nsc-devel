"""Keep class aliases readonly when obtained from a static ref-readonly result."""

from html import escape
from pathlib import Path
import subprocess
import sys

from isolated_a_runner import absolute_a, setup


PRODUCER = '''public class Payload { public int Data; public Payload() { Data = 42; } public void Mutate() { Data = 7; } }
public struct Owner { public owned Payload item; public Owner() { item = new Payload(); } }
public class Access {
 public static Payload Global;
 public static ref readonly Payload Pick(ref Owner owner) { return ref owner.item; }
 public static ref Payload Mutable(ref Owner owner) { return ref owner.item; }
 public static ref readonly[static] Payload StaticPick() { return ref Global; }
 public ref readonly[static] Payload InstancePick() { return ref Global; }
 public static ref[static] Payload StaticMutable() { return ref Global; }
}
'''
CASES = {
    'alias': ('public static int Main() { Owner owner = new Owner(); Payload alias = Access.Pick(ref owner); alias.Data = 7; return alias.Data; }',
              'borrowed value cannot be stored in unqualified local'),
    'direct': ('public static int Main() { Owner owner = new Owner(); Access.Pick(ref owner).Data = 7; return 42; }',
               'left side of assignment is not assignable'),
    'method': ('public static int Main() { Owner owner = new Owner(); Access.Pick(ref owner).Mutate(); return 42; }',
               'readonly borrowed object cannot invoke'),
    'pass_mutable': ('public static void Mutate(Payload value) { value.Mutate(); } public static int Main() { Owner owner = new Owner(); Mutate(Access.Pick(ref owner)); return 42; }',
                     'readonly borrowed object'),
    'return': ('public static Payload Leak(ref Owner owner) { return Access.Pick(ref owner); } public static int Main() { return 42; }',
               'borrowed value cannot escape'),
    'global': ('public static int Main() { Owner owner = new Owner(); Global = Access.Pick(ref owner); return 42; }',
               'borrowed value cannot be stored in field'),
    'borrowed_alias': ('public static int Main() { Owner owner = new Owner(); borrowed Payload alias = Access.Pick(ref owner); return alias.Data; }',
                       'borrowed local requires a direct'),
    'static_lifetime_alias': ('public static int Main() { Access.Global = new Payload(); Payload alias = Access.StaticPick(); alias.Data = 7; return alias.Data; }',
                              'borrowed value cannot be stored in unqualified local'),
    'instance_lifetime_alias': ('public static int Main() { Access.Global = new Payload(); Access access = new Access(); Payload alias = access.InstancePick(); alias.Data = 7; return alias.Data; }',
                                'borrowed value cannot be stored in unqualified local'),
    'static_lifetime_method': ('public static int Main() { Access.Global = new Payload(); Access.StaticPick().Mutate(); return 42; }',
                               'readonly borrowed object cannot invoke'),
    'instance_lifetime_method': ('public static int Main() { Access.Global = new Payload(); Access access = new Access(); access.InstancePick().Mutate(); return 42; }',
                                 'readonly borrowed object cannot invoke'),
    'static_lifetime_read': ('public static int Main() { Access.Global = new Payload(); return Access.StaticPick().Data; }', None),
    'instance_lifetime_read': ('public static int Main() { Access.Global = new Payload(); Access access = new Access(); return access.InstancePick().Data; }', None),
    'static_lifetime_mutable': ('public static int Main() { Access.Global = new Payload(); Access.StaticMutable().Data = 42; return Access.Global.Data; }', None),
    'read': ('public static int Main() { Owner owner = new Owner(); return Access.Pick(ref owner).Data; }', None),
    'mutable': ('public static int Main() { Owner owner = new Owner(); Access.Mutable(ref owner).Data = 42; return owner.item.Data; }', None),
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
        for name, (body, diagnostic) in CASES.items():
            case = root / (producer_level + '-' + name)
            case.mkdir()
            program = 'class Program { public static Payload Global; ' + body + ' }\n'
            (case / 'Main.ns').write_text((PRODUCER if producer_level == 'source' else '') + program, encoding='utf-8')
            reference = '' if producer_level == 'source' else (
                '<LibraryReference Include="' + escape(library.as_posix(), quote=True) +
                '" Metadata="' + escape(library.with_suffix('.nsmeta').as_posix(), quote=True) + '"/>')
            project = case / 'Probe.nsproj'
            project.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup><ItemGroup>'
                               '<Compile Include="Main.ns"/>' + reference + '</ItemGroup></Project>', encoding='utf-8')
            for level in ('O0', 'O2'):
                executable = case / (level + '.exe')
                command = [compiler, '--check-project-method-bodies', project] if diagnostic else [
                    compiler, '--emit-project-oop-exe', project, '-' + level, '-o', executable, '--no-artifact-cache']
                built = invoke(command, repo, environment)
                if diagnostic:
                    if built.returncode == 0 or diagnostic not in built.stdout + built.stderr:
                        failures.append((producer_level, name, level, 'expected rejection', built.stdout + built.stderr))
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
