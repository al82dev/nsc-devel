"""Exercise dependent interface constraints with the actual project compiler."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile


TYPES = """public interface Reducer<T> { public T Apply(T value); }
public class IntReducer : Reducer<int> {
    public IntReducer() { }
    public int Apply(int value) { return value; }
}
public class LongReducer : Reducer<long> {
    public long Apply(long value) { return value; }
}
"""


def report(message):
    """Emit progress and optionally preserve the same text in a new log."""
    print(message, flush=True)
    log = os.environ.get('NSC_DIRECTED_LOG')
    if log:
        with Path(log).open('a', encoding='utf-8') as output:
            output.write(message + '\n')


def invoke(command, root, label):
    """Run one directed compiler or executable command and retain its output."""
    report('START ' + label)
    process = subprocess.Popen([str(part) for part in command], cwd=root,
                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                               text=True, errors='replace')
    while True:
        try:
            output, _ = process.communicate(timeout=25)
            break
        except subprocess.TimeoutExpired:
            report('RUNNING ' + label)
    result = subprocess.CompletedProcess(command, process.returncode, output)
    report('END ' + label + ' exit=' + str(result.returncode))
    if result.stdout:
        report(result.stdout)
    return result


def write_project(root, name, body):
    """Create one isolated project from the explicit source for this case."""
    root.mkdir()
    (root / 'Probe.nsproj').write_text(
        '<Project><PropertyGroup><Name>' + name + '</Name><Language>3</Language>'
        '</PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
        '</ItemGroup></Project>', encoding='utf-8')
    (root / 'Main.ns').write_text(body, encoding='utf-8')
    return root / 'Probe.nsproj'


def check_case(compiler, root, name, body, expected, diagnostic=None):
    """Check one declaration and emit the requested O0/O2 variants."""
    project = write_project(root / name, name, body)
    check = invoke([compiler, '--check-project-method-bodies', project], root, name + ':check')
    if diagnostic is None:
        assert check.returncode == 0, check.stdout
    else:
        assert check.returncode != 0 and diagnostic in check.stdout, check.stdout
        assert "unknown type name 'T'" not in check.stdout, check.stdout
    if os.name != 'nt':
        raise RuntimeError('O0/O2 executable validation requires Windows')
    for level in ('O0', 'O2'):
        executable = project.parent / (name + '-' + level + '.exe')
        emitted = invoke([compiler, '--emit-project-oop-exe', project, '-o', executable,
                          '--no-artifact-cache', '-' + level], root, name + ':' + level)
        if diagnostic is not None:
            assert emitted.returncode != 0 and diagnostic in emitted.stdout, emitted.stdout
            assert "unknown type name 'T'" not in emitted.stdout, emitted.stdout
            continue
        assert emitted.returncode == 0, emitted.stdout
        result = invoke([executable], root, name + ':' + level + ':run')
        assert result.returncode == expected, result.stdout


def main():
    """Validate two matching types, a mismatch, and non-dependent controls."""
    if len(sys.argv) != 2:
        raise SystemExit('usage: generic_dependent_constraint.py <compiler>')
    log = os.environ.get('NSC_DIRECTED_LOG')
    if log:
        Path(log).open('x', encoding='utf-8').close()
    compiler = Path(sys.argv[1])
    dependent = ('public static int Use<T, TOp>() where TOp : Reducer<T> '
                 '{ return 21; }')
    valid = TYPES + 'class Program { ' + dependent + (
        ' public static int Main() { return Use<int, IntReducer>() + '
        'Use<long, LongReducer>(); } }')
    invalid = TYPES + 'class Program { ' + dependent + (
        ' public static int Main() { return Use<long, IntReducer>(); } }')
    invalid_reverse = TYPES + 'class Program { ' + dependent + (
        ' public static int Main() { return Use<int, LongReducer>(); } }')
    type_template = ('class Container<T, TOp> where TOp : Reducer<T> '
                     '{ public static int Read() { return 42; } } ')
    type_valid = TYPES + type_template + ('class Program { public static int Main() '
        '{ return Container<int, IntReducer>.Read(); } }')
    type_invalid = TYPES + type_template + ('class Program { public static int Main() '
        '{ return Container<long, IntReducer>.Read(); } }')
    invoke_reducer = TYPES + ('class Program { public static T Use<T, TOp>(TOp op, T value) '
        'where TOp : Reducer<T> { return op.Apply(value); } public static int Main() '
        '{ raii IntReducer op = new IntReducer(); return Use<int, IntReducer>(op, 42); } }')
    closed = TYPES + ('class Program { public static int Use<TOp>() '
        'where TOp : Reducer<int> { return 42; } '
        'public static int Main() { return Use<IntReducer>(); } }')
    open_unused = TYPES + 'class Program { ' + dependent + (
        ' public static int Main() { return 42; } }')
    invalid_template = ('struct NotInterface<T> { } class Program { '
        'public static int Use<T, TOp>() where TOp : NotInterface<T> { return 1; } '
        'public static int Main() { return 42; } }')
    missing_template = ('class Program { public static int Use<T, TOp>() '
        'where TOp : Missing<T> { return 1; } public static int Main() { return 42; } }')
    missing_argument = ('interface PairReducer<T, U> { } class Program { '
        'public static int Use<T, TOp>() where TOp : PairReducer<T, Missing> '
        '{ return 1; } public static int Main() { return 42; } }')
    alpha = TYPES + ('class Program { '
        'public static int Select<T, TOp>(int value) where TOp : Reducer<T> { return 1; } '
        'public specialize static int Select<U, UOp>(int value) '
        'where UOp : Reducer<U>, new() { return 42; } '
        'public static int Main() { return Select<int, IntReducer>(1); } }')
    different_position = TYPES + ('class Program { '
        'public static int Select<T, U, TOp>(int x) where TOp : Reducer<T> { return 1; } '
        'public specialize static int Select<A, B, Op>(int x) '
        'where Op : Reducer<B>, new() { return 42; } '
        'public static int Main() { return Select<int, long, IntReducer>(1); } }')
    unused = 'class Program { public static int Main() { return 42; } }'
    with tempfile.TemporaryDirectory(prefix='nsc-dependent-constraints-') as temporary:
        root = Path(temporary)
        check_case(compiler, root, 'Valid', valid, 42)
        check_case(compiler, root, 'Invalid', invalid, None, 'does not implement')
        check_case(compiler, root, 'InvalidReverse', invalid_reverse, None, 'does not implement')
        check_case(compiler, root, 'TypeValid', type_valid, 42)
        check_case(compiler, root, 'TypeInvalid', type_invalid, None, 'does not implement')
        check_case(compiler, root, 'InvokeReducer', invoke_reducer, 42)
        check_case(compiler, root, 'Closed', closed, 42)
        check_case(compiler, root, 'OpenUnused', open_unused, 42)
        check_case(compiler, root, 'InvalidTemplate', invalid_template, None, 'cannot constrain')
        check_case(compiler, root, 'MissingTemplate', missing_template, None, 'does not name a unique')
        check_case(compiler, root, 'MissingArgument', missing_argument, None, "unknown type name 'Missing'")
        check_case(compiler, root, 'Alpha', alpha, 42)
        check_case(compiler, root, 'DifferentPosition', different_position, None, 'incomparable constraints')
        check_case(compiler, root, 'Unused', unused, 42)
    return 0


if __name__ == '__main__':
    sys.exit(main())
