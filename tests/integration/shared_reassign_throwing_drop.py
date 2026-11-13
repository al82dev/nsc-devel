"""Verify constructor-field replacement when destruction or construction fails."""

from pathlib import Path
import sys

from isolated_a_runner import absolute_a, setup
import shared_throwing_value_field_proven as fixture


def make_source(kind, family, owned, mode, scenario):
    source = fixture.build_source(kind, family, owned, 'reassign')
    initializer = 'Item = new Thing();' if owned else ''
    source = source.replace('struct Field {', '''class Gate {
 static Gate() throws Exception { throw new Exception(); }
 public static int Fail() { return 1; } }
struct Field { public static bool Thrown;''')
    if scenario == 'destructor':
        before = 'DropTrace = DropTrace * 10 + Id; } }'
        after = ('DropTrace = DropTrace * 10 + Id; '
                 'if (Id == 1 && !Thrown) { Thrown = true; int ignored = Gate.Fail(); } } }')
        expected_builds, expected_drops, expected_trace = 2, 2, 12
    else:
        before = 'Id = Builds; ' + initializer + ' }'
        after = ('Id = Builds; ' + initializer +
                 ' if (Id == 2) { Thrown = true; int ignored = Gate.Fail(); } }')
        expected_builds, expected_drops, expected_trace = 2, 1, 1
    if before not in source:
        raise ValueError('fixture shape changed')
    source = source.replace(before, after)
    if mode == 'direct':
        declaration = 'owned Bad value = new Bad();' if kind == 'class' else 'Bad value = new Bad();'
        source = source.replace(f'{family}<Bad> value = SharedPointers.Make{family}Throwing<Bad>();', declaration)
    start = source.index(' return caught ==')
    thing = expected_builds if owned else 0
    return source[:start] + f''' return caught == 1 && Field.Thrown && Bad.Drops == 0 &&
 Field.Builds == {expected_builds} && Field.Drops == {expected_drops} && Field.DropTrace == {expected_trace} &&
 Thing.Builds == {thing} && Thing.Drops == {thing} ? 42 : 20; }} }}'''


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
                for mode in ('direct', 'shared'):
                    for scenario in ('destructor', 'constructor'):
                        name = f'{kind}-{family}-owned{int(owned)}-{mode}-{scenario}'
                        fixture.run_case(compiler, root, core, metadata, name,
                            make_source(kind, family, owned, mode, scenario), False,
                            repository, environment, failures)
    for kind, family in (('class', 'Rc'), ('struct', 'Arc')):
        for mode in ('direct', 'shared'):
            source = make_source(kind, family, True, mode, 'destructor')
            source = source.replace('struct Field {', '''class Gate2 {
 static Gate2() throws Exception { throw new Exception(); }
 public static int Fail() { return 1; } }
struct Field {''')
            source = source.replace('int ignored = Gate.Fail(); } } }',
                'int ignored = Gate.Fail(); } if (Id == 2) { int ignored = Gate2.Fail(); } } }')
            before = len(failures)
            fixture.run_case(compiler, root, core, metadata, kind + '-' + mode + '-double-throw',
                source, False, repository, environment, failures)
            observed = failures[before:]
            if len(observed) == 2 and all(item[2] == 'run' and item[3] == 148 for item in observed):
                del failures[before:]
                print('EXPECTED TRAP', kind, mode, 148, flush=True)
    for failure in failures:
        print('FAIL', *failure, file=sys.stderr)
    return int(bool(failures))


if __name__ == '__main__':
    sys.exit(main())
