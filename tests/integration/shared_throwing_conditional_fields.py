"""Check conditional field replacement before shared construction completes."""

from pathlib import Path
import sys

from isolated_a_runner import absolute_a, setup
from shared_throwing_value_field_proven import build_source, run_case
from shared_reassign_throwing_drop import make_source as make_throwing_replacement


def source_for(kind, family, owned, flag, throws, with_else):
    variant = 'two_fields_' + ('reassign' if flag or with_else else '')
    variant += '' if throws else '_success'
    variant = variant.replace('two_fields__', 'two_fields_')
    source = build_source(kind, family if family != 'direct' else 'Rc', owned, variant)
    statement = 'if (Flag) { field = new Field(); }'
    if with_else:
        statement += ' else { other = new Field(); }'
    prefix = 'public Bad() throws Exception { '
    old = 'field = new Field(); ' if flag or with_else else ''
    source = source.replace(prefix + old, prefix + statement + ' ')
    if with_else and not flag:
        source = source.replace('Field.DropTrace == 123', 'Field.DropTrace == 231')
    if flag:
        source = source.replace('int caught = 0;', 'int caught = 0; Bad.Flag = true;')
    if family == 'direct':
        declaration = 'owned Bad value = new Bad();' if kind == 'class' else 'Bad value = new Bad();'
        source = source.replace('Rc<Bad> value = SharedPointers.MakeRcThrowing<Bad>();', declaration)
    return source


def main():
    if len(sys.argv) != 4:
        raise SystemExit('expected nsc, Core.lib and a new evidence directory on A:')
    compiler, core, root = (absolute_a(value) for value in sys.argv[1:])
    metadata = core.with_suffix('.nsmeta')
    if not core.is_file() or not metadata.is_file():
        raise SystemExit('Core library and metadata must exist')
    repo = Path(__file__).resolve().parents[2]
    environment = setup(root, repo)
    failures = []
    for kind in ('class', 'struct'):
        for family in ('direct', 'Rc', 'Arc'):
            for owned in (False, True):
                for flag in (False, True):
                    for throws in (False, True):
                        for with_else in (False, True):
                            source = source_for(kind, family, owned, flag, throws, with_else)
                            name = f'{kind}-{family}-{int(owned)}-{int(flag)}-{int(throws)}-{int(with_else)}'
                            run_case(compiler, root, core, metadata, name, source, False, repo, environment, failures)
        for family in ('Rc', 'Arc'):
            source = build_source(kind, family, False, 'branch_nested')
            run_case(compiler, root, core, metadata, f'{kind}-{family}-nested', source,
                     True, repo, environment, failures)
            source = source_for(kind, family, False, True, True, False)
            source = source.replace('public static bool Flag;',
                                    'public static bool Flag; public static bool Check() { return Flag; }')
            source = source.replace('if (Flag) { field = new Field(); }',
                                    'if (Check()) { field = new Field(); }')
            run_case(compiler, root, core, metadata, f'{kind}-{family}-call-condition', source,
                     True, repo, environment, failures)
        for family, mode in (('Rc', 'direct'), ('Rc', 'shared'), ('Arc', 'shared')):
            for scenario in ('constructor', 'destructor'):
                source = make_throwing_replacement(kind, family, True, mode, scenario)
                source = source.replace('field = new Field(); throw new Exception();',
                                        'if (Flag) { field = new Field(); } throw new Exception();')
                source = source.replace('int caught = 0;', 'int caught = 0; Bad.Flag = true;')
                run_case(compiler, root, core, metadata, f'{kind}-{family}-{mode}-throwing-{scenario}',
                         source, False, repo, environment, failures)
    for failure in failures:
        print('FAIL', *failure, file=sys.stderr)
    return int(bool(failures))


if __name__ == '__main__':
    sys.exit(main())
