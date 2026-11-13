"""Check both branches of throwing constructors in shared factories."""

from pathlib import Path
import sys

from isolated_a_runner import absolute_a, setup
from shared_throwing_value_field_proven import build_source, run_case


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
                for fields in (1, 2):
                    for flag in (False, True):
                        variant = 'branch_else' if fields == 1 else 'two_fields_branch_else'
                        source = build_source(kind, family if family != 'direct' else 'Rc', owned, variant)
                        if family == 'direct':
                            declaration = 'owned Bad value = new Bad();' if kind == 'class' else 'Bad value = new Bad();'
                            source = source.replace('Rc<Bad> value = SharedPointers.MakeRcThrowing<Bad>();', declaration)
                        if flag:
                            source = source.replace('int caught = 0;', 'int caught = 0; Bad.Flag = true;')
                        name = f'{kind}-{family}-{int(owned)}-{fields}-{int(flag)}'
                        run_case(compiler, root, core, metadata, name, source, False, repo, environment, failures)
        for family in ('Rc', 'Arc'):
            source = build_source(kind, family, False, 'branch_else')
            source = source.replace('else throw new Exception();', 'else { field = new Field(); throw new Exception(); }')
            run_case(compiler, root, core, metadata, f'{kind}-{family}-else-mutates', source,
                     True, repo, environment, failures)
    for failure in failures:
        print('FAIL', *failure, file=sys.stderr)
    return int(bool(failures))


if __name__ == '__main__':
    sys.exit(main())
