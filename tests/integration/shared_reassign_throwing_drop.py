"""Verify constructor-field replacement when destruction or construction fails."""

from pathlib import Path
import os
import sys
import tempfile

from isolated_a_runner import absolute_a, setup
import shared_throwing_value_field_proven as fixture


def make_source(kind, family, owned, mode, scenario):
    source = fixture.build_source(kind, family, owned, 'reassign')
    if scenario.startswith('moved_'):
        if 'field = new Field(); throw new Exception();' not in source:
            raise ValueError('moved fixture shape changed')
        source = source.replace('field = new Field(); throw new Exception();',
            'Field replacement = new Field(); field = move(replacement); throw new Exception();')
        scenario = scenario.removeprefix('moved_')
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


def make_multiple_local_failure(kind, family, mode, first_moved):
    source = fixture.build_source(kind, family, True, 'two_fields')
    source = source.replace('struct Field {', '''class Gate {
 static Gate() throws Exception { throw new Exception(); }
 public static int Fail() { return 1; } }
struct Field {''')
    before = 'Id = Builds; Item = new Thing(); }'
    if before not in source:
        raise ValueError('multiple moved locals failing constructor fixture changed')
    source = source.replace(before, 'Id = Builds; Item = new Thing(); '
        'if (Id == 4) { int ignored = Gate.Fail(); } }')
    before = 'public Bad() throws Exception { throw new Exception(); }'
    middle = 'field = move(left); ' if first_moved else ''
    after = ('public Bad() throws Exception { Field left = new Field(); ' + middle +
             'Field right = new Field(); ' + ('' if first_moved else 'field = move(left); ') +
             'other = move(right); throw new Exception(); }')
    if before not in source:
        raise ValueError('multiple moved locals constructor fixture changed')
    source = source.replace(before, after)
    if mode == 'direct':
        declaration = 'owned Bad value = new Bad();' if kind == 'class' else 'Bad value = new Bad();'
        source = source.replace(f'{family}<Bad> value = SharedPointers.Make{family}Throwing<Bad>();', declaration)
    start = source.index(' return caught ==')
    trace = 123 if first_moved else 321
    return source[:start] + f''' return caught == 1 && Bad.Drops == 0 &&
 Field.Builds == 4 && Field.Drops == 3 && Field.DropTrace == {trace} &&
 Thing.Builds == 4 && Thing.Drops == 4 ? 42 : 20; }} }}'''


def make_multiple_local_drop_failure(kind, family, mode):
    source = fixture.build_source(kind, family, True, 'two_fields')
    if ('DropTrace = DropTrace * 10 + Id; } }' not in source or
        'public Bad() throws Exception { throw new Exception(); }' not in source):
        raise ValueError('multiple live locals destructor fixture changed')
    source = source.replace('struct Field {', '''class Gate {
 static Gate() throws Exception { throw new Exception(); }
 public static int Fail() { return 1; } }
struct Field { public static bool Thrown;''')
    source = source.replace('DropTrace = DropTrace * 10 + Id; } }',
        'DropTrace = DropTrace * 10 + Id; if (Id == 1 && !Thrown) '
        '{ Thrown = true; int ignored = Gate.Fail(); } } }')
    source = source.replace('public Bad() throws Exception { throw new Exception(); }',
        'public Bad() throws Exception { Field left = new Field(); Field right = new Field(); '
        'field = move(left); other = move(right); throw new Exception(); }')
    if mode == 'direct':
        declaration = 'owned Bad value = new Bad();' if kind == 'class' else 'Bad value = new Bad();'
        source = source.replace(f'{family}<Bad> value = SharedPointers.Make{family}Throwing<Bad>();', declaration)
    start = source.index(' return caught ==')
    return source[:start] + ''' return caught == 1 && Field.Thrown && Bad.Drops == 0 &&
 Field.Builds == 4 && Field.Drops == 4 && Field.DropTrace == 1342 &&
 Thing.Builds == 4 && Thing.Drops == 4 ? 42 : 20; } }'''


def run(compiler, core, metadata, root):
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
        for owned in (False, True):
            for family, mode in (('Rc', 'direct'), ('Rc', 'shared'), ('Arc', 'shared')):
                for scenario in ('moved_destructor', 'moved_constructor'):
                    name = f'{kind}-{family}-owned{int(owned)}-{mode}-{scenario}'
                    fixture.run_case(compiler, root, core, metadata, name,
                        make_source(kind, family, owned, mode, scenario), False,
                        repository, environment, failures)
                for variant in ('reassign_success', 'reassign'):
                    source = fixture.build_source(kind, family, owned, variant)
                    body = '' if variant.endswith('success') else 'throw new Exception();'
                    before = 'public Bad() throws Exception { field = new Field(); ' + body + ' }'
                    if before not in source:
                        raise ValueError('moved fixture shape changed')
                    source = source.replace(before, 'public Bad() throws Exception { '
                        'Field replacement = new Field(); field = move(replacement); ' + body + ' }')
                    if mode == 'direct':
                        declaration = 'owned Bad value = new Bad();' if kind == 'class' else 'Bad value = new Bad();'
                        source = source.replace(f'{family}<Bad> value = SharedPointers.Make{family}Throwing<Bad>();', declaration)
                    fixture.run_case(compiler, root, core, metadata,
                        f'{kind}-{family}-owned{int(owned)}-{mode}-moved-{variant}',
                        source, False, repository, environment, failures)
                for success in (False, True):
                    source = fixture.build_source(kind, family, owned,
                        'two_fields_success' if success else 'two_fields')
                    old_body = '' if success else 'throw new Exception();'
                    before = 'public Bad() throws Exception { ' + old_body + ' }'
                    after = ('public Bad() throws Exception { Field left = new Field(); Field right = new Field(); '
                             'field = move(left); other = move(right); ' + old_body + ' }')
                    if before not in source:
                        raise ValueError('multiple moved locals fixture shape changed')
                    source = source.replace(before, after)
                    source = source.replace('Field.Builds == 2', 'Field.Builds == 4')
                    source = source.replace('Field.Drops == 2', 'Field.Drops == 4')
                    source = source.replace('Field.DropTrace == 21', 'Field.DropTrace == 1243')
                    if owned:
                        source = source.replace('Thing.Builds == 2', 'Thing.Builds == 4')
                        source = source.replace('Thing.Drops == 2', 'Thing.Drops == 4')
                    if mode == 'direct':
                        declaration = 'owned Bad value = new Bad();' if kind == 'class' else 'Bad value = new Bad();'
                        source = source.replace(f'{family}<Bad> value = SharedPointers.Make{family}Throwing<Bad>();', declaration)
                    fixture.run_case(compiler, root, core, metadata,
                        f'{kind}-{family}-owned{int(owned)}-{mode}-two-moved-{int(success)}',
                        source, False, repository, environment, failures)
            source = fixture.build_source(kind, 'Rc', owned, 'reassign')
            source = source.replace('field = new Field(); throw new Exception();',
                                    'Field replacement = new Field(); throw new Exception();')
            fixture.run_case(compiler, root, core, metadata, f'{kind}-owned{int(owned)}-unconsumed',
                source, True, repository, environment, failures)
        for family, mode in (('Rc', 'direct'), ('Rc', 'shared'), ('Arc', 'shared')):
            for first_moved in (False, True):
                fixture.run_case(compiler, root, core, metadata,
                    f'{kind}-{family}-{mode}-two-moved-failing-{int(first_moved)}',
                    make_multiple_local_failure(kind, family, mode, first_moved),
                    False, repository, environment, failures)
            fixture.run_case(compiler, root, core, metadata, f'{kind}-{family}-{mode}-two-live-drop-failure',
                make_multiple_local_drop_failure(kind, family, mode), False,
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


def main():
    if os.name != 'nt':
        return 77
    if len(sys.argv) == 5:
        return run(*(absolute_a(value) for value in sys.argv[1:]))
    if len(sys.argv) != 2:
        raise SystemExit('expected nsc or nsc, Core.lib, Core.nsmeta and a new evidence directory on A:')
    core_name = os.environ.get('NSC_TEST_CORE_BINARY')
    if not core_name:
        print('SKIP: NSC_TEST_CORE_BINARY is not configured', flush=True)
        return 77
    compiler = Path(sys.argv[1]).resolve()
    core = Path(core_name).resolve()
    temporary = Path(tempfile.gettempdir()).resolve()
    if any(path.drive.upper() != 'A:' for path in (compiler, core, temporary)):
        print('SKIP: this focused regression requires the compiler, Core and temporary directory on A:', flush=True)
        return 77
    metadata = core.with_suffix('.nsmeta')
    if not compiler.is_file() or not core.is_file() or not metadata.is_file():
        raise SystemExit('configured compiler, Core or metadata is missing')
    with tempfile.TemporaryDirectory(prefix='nsc-shared-reassign-', dir=temporary) as directory:
        return run(compiler, core, metadata, Path(directory) / 'cases')


if __name__ == '__main__':
    sys.exit(main())
