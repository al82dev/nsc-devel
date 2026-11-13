"""Focused source and binary-Core checks for throwing shared payload construction."""

from html import escape
from pathlib import Path
import subprocess
import sys

from isolated_a_runner import absolute_a, setup


SOURCE = '''using NativeSharp.Core;
class Thing { public static int Builds; public static int Drops;
 public Thing() { Builds = Builds + 1; } ~Thing() { Drops = Drops + 1; } }
struct Field { public static int Builds; public static int Drops; public static int DropTrace; public int Id; FIELD
 public Field() { Builds = Builds + 1; Id = Builds; INIT }
 ~Field() { Drops = Drops + 1; DropTrace = DropTrace * 10 + Id; } }
BASE
KIND Bad INHERIT { public static int Drops; public static bool Flag;
 public Field field = new Field(); EXTRA
 public Bad() throws Exception { BODY }
 ~Bad() { Drops = Drops + 1; } }
class Program { public static int Main() { int caught = 0;
 try { FAMILY<Bad> value = SharedPointers.FACTORY<Bad>(); }
 catch (Exception error) { caught = 1; }
 return caught == CAUGHT && Field.Builds == BUILDS && Field.Drops == DROPS && Field.DropTrace == DROPTRACE && Bad.Drops == BADDROPS &&
  Thing.Builds == THINGBUILDS && Thing.Drops == THINGDROPS BASECHECK ? 42 : 20; } }
'''


def run_case(compiler, root, core, metadata, name, source, expected_reject, repository, environment, failures):
    case = root / name
    case.mkdir()
    (case / 'Main.ns').write_text(source, encoding='utf-8')
    project = case / 'Probe.nsproj'
    project.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup><ItemGroup>'
        '<Compile Include="Main.ns" /><LibraryReference Include="' + escape(core.as_posix(), quote=True) +
        '" Metadata="' + escape(metadata.as_posix(), quote=True) + '" /></ItemGroup></Project>', encoding='utf-8')
    for level in ('O0', 'O2'):
        exe = case / (level + '.exe')
        command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level,
                   '-o', str(exe), '--no-artifact-cache']
        print('COMMAND', subprocess.list2cmdline(command), 'CWD', repository, flush=True)
        result = subprocess.run(command, cwd=repository, env=environment, capture_output=True,
                                text=True, errors='replace', timeout=120)
        output = result.stdout + result.stderr
        if expected_reject:
            accepted = result.returncode == 0 or 'shared throwing construction cannot safely clean' not in output
            print('REJECT', name, level, result.returncode, flush=True)
            if accepted:
                failures.append((name, level, 'expected barrier', output))
            continue
        if result.returncode:
            failures.append((name, level, 'compile', output))
            continue
        result = subprocess.run([str(exe)], cwd=repository, env=environment, capture_output=True,
                                text=True, errors='replace', timeout=10)
        print('RUN', name, level, result.returncode, flush=True)
        if result.returncode != 42:
            failures.append((name, level, 'run', result.returncode))


def build_source(kind, family, owned, variant):
    base_failure = variant == 'base_failure'
    count = 0 if base_failure else 3 if variant.startswith('three_fields') else 2 if variant.startswith('two_fields') else 1
    extra = 'public Field other = new Field();' if count > 1 else ''
    if count > 2:
        extra += ' public Field third = new Field();'
    values = {'KIND': 'class' if base_failure else kind,
              'FAMILY': family, 'FACTORY': 'MakeRcThrowing' if family == 'Rc' else 'MakeArcThrowing',
              'FIELD': 'public owned Thing Item;' if owned else '', 'INIT': 'Item = new Thing();' if owned else '',
              'BASE': 'class Base { public static int Calls; public Base() throws Exception '
                      '{ Calls = Calls + 1; throw new Exception(); } }' if base_failure else '',
              'INHERIT': ': Base' if base_failure else '',
              'EXTRA': extra,
              'BODY': 'if (Flag) throw new Exception();' if variant == 'branch' else
                      'field = new Field(); throw new Exception();' if variant == 'reassign' else
                      '' if variant.endswith('success') else 'throw new Exception();',
              'CAUGHT': '0' if variant.endswith('success') else '1',
              'BADDROPS': '1' if variant.endswith('success') else '0',
              'BUILDS': str(count), 'DROPS': str(count),
              'DROPTRACE': ('0', '1', '21', '321')[count],
              'THINGBUILDS': str(count if owned else 0),
              'THINGDROPS': str(count if owned else 0),
              'BASECHECK': '&& Base.Calls == 1' if base_failure else ''}
    source = SOURCE
    for key in sorted(values, key=len, reverse=True):
        source = source.replace(key, values[key])
    return source


def main():
    if len(sys.argv) != 5:
        raise SystemExit('expected nsc, Core.lib, Core.nsmeta and a new evidence directory on A:')
    compiler, core, metadata, root = (absolute_a(value) for value in sys.argv[1:])
    repository = Path(__file__).resolve().parents[2]
    environment = setup(root, repository)
    failures = []
    for kind in ('class', 'struct'):
        for family in ('Rc', 'Arc'):
            for owned in (False, True):
                name = f'{kind}-{family}-owned{int(owned)}'
                run_case(compiler, root, core, metadata, name, build_source(kind, family, owned, 'normal'),
                         False, repository, environment, failures)
                run_case(compiler, root, core, metadata, name + '-success',
                         build_source(kind, family, owned, 'success'), False,
                         repository, environment, failures)
                for variant in ('two_fields', 'two_fields_success', 'three_fields', 'three_fields_success'):
                    run_case(compiler, root, core, metadata, name + '-' + variant,
                             build_source(kind, family, owned, variant), False,
                             repository, environment, failures)
            for variant in ('branch', 'reassign'):
                name = f'{kind}-{family}-{variant}'
                run_case(compiler, root, core, metadata, name, build_source(kind, family, False, variant),
                         True, repository, environment, failures)
    for family in ('Rc', 'Arc'):
        run_case(compiler, root, core, metadata, 'base-failure-' + family,
                 build_source('class', family, True, 'base_failure'), False, repository, environment, failures)
    for failure in failures:
        print('FAIL', *failure, file=sys.stderr)
    return int(bool(failures))


if __name__ == '__main__':
    sys.exit(main())
