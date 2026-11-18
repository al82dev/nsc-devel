"""Check the existing SortWith semantic fixture with binary SDK libraries."""

import ast
import os
from pathlib import Path
import sys
import tempfile
from xml.sax.saxutils import quoteattr

import sort_with_binary_original as common


def fixture_source(path):
    """Extract the unmodified source string from the existing integration test."""
    tree = ast.parse(path.read_text(encoding='utf-8'))
    for function in tree.body:
        if isinstance(function, ast.FunctionDef) and function.name == 'main':
            for statement in function.body:
                if isinstance(statement, ast.Assign):
                    if any(isinstance(target, ast.Name) and target.id == 'source'
                           for target in statement.targets):
                        return ast.literal_eval(statement.value)
    raise ValueError('the existing semantic fixture source was not found')


def write_project(root, source, args):
    """Create a consumer of the given immutable binary libraries in a private folder."""
    root.mkdir()
    (root / 'Main.ns').write_text(source, encoding='utf-8', newline='')
    text = '<Project><PropertyGroup><Name>SortWithBinarySemantics</Name>'
    text += '<OutputType>Exe</OutputType></PropertyGroup>'
    text += '<ItemGroup><Compile Include="Main.ns" />'
    references = ((args.core_library, args.core_metadata),
                  (args.collections_library, args.collections_metadata))
    for library, metadata in references:
        text += '<LibraryReference Include=' + quoteattr(library.resolve().as_posix())
        text += ' Metadata=' + quoteattr(metadata.resolve().as_posix()) + ' />'
    text += '</ItemGroup></Project>'
    project = root / 'Consumer.nsproj'
    project.write_text(text, encoding='utf-8')
    return project


def main():
    """Test inferred calls and a separate explicit-type control in each mode."""
    args = common.arguments()
    repo = Path(__file__).resolve().parents[2]
    inputs = [args.compiler, args.original, args.core_library, args.core_metadata,
              args.collections_library, args.collections_metadata]
    if args.asan_clang:
        inputs.extend([args.asan_clang, args.asan_runtime])
    if not args.temp_root.is_dir() or any(not path.is_file() for path in inputs):
        print('ERROR: a required input or temporary directory is missing', file=sys.stderr)
        return 2
    before = {str(path.resolve()): common.digest(path) for path in inputs}
    source = fixture_source(args.original)
    call = 'Algorithm.SortWith('
    if source.count(call) != 2:
        print('ERROR: expected exactly two inferred SortWith calls', file=sys.stderr)
        return 2
    explicit = source.replace(call, 'Algorithm.SortWith<int>(')
    print('INPUT_SHA256_BEFORE', before, flush=True)
    print('MODE', args.mode, 'ASAN_OPTIONS', os.environ.get('ASAN_OPTIONS', '<not set>'), flush=True)
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-sortwith-semantics-', dir=args.temp_root) as temp:
        root = Path(temp)
        modes = ('normal', 'asan') if args.mode == 'both' else (args.mode,)
        for case, text in (('inferred', source), ('explicit', explicit)):
            folder = root / case
            project = write_project(folder, text, args)
            if (folder / 'Main.ns').read_bytes() != text.encode('utf-8'):
                failures.append(case + ': generated source differs from fixture')
                continue
            print('CASE', case, 'SOURCE_SHA256', common.digest(folder / 'Main.ns'), flush=True)
            for mode in modes:
                for level in ('O0', 'O2'):
                    executable = folder / (mode + '-' + level + '.exe')
                    command = [args.compiler.resolve(), '--emit-project-oop-exe', project, '-' + level,
                               '-o', executable, '--no-artifact-cache', '--no-error-log']
                    if mode == 'asan':
                        command += ['--sanitize-address', '--asan-clang=' + str(args.asan_clang.resolve()),
                                    '--asan-runtime=' + str(args.asan_runtime.resolve())]
                    code, output = common.run(command, repo)
                    if code != 0:
                        failures.append(case + ' ' + mode + ' ' + level + ': compilation failed')
                        continue
                    code, output = common.run([executable], repo)
                    if code != 42 or 'ERROR: AddressSanitizer' in output:
                        failures.append(case + ' ' + mode + ' ' + level + ': execution failed')
        # A same-named generic type must not acquire Core.Span's identity merely
        # because an imported parameter is written with a module qualifier.
        counterfeit = '''using NativeSharp.Core;
using NativeSharp.Core.Collections.Generic;
namespace Other {
 public struct Span<T> { }
 public class Spans { public static Span<T> Of<T>(T[] values) { return default(Span<T>); } }
}
class Order { public static int Compare(int left, int right) { return left - right; } }
class Program { public static int Main() {
 owned int[] values = new int[1]; Comparison<int> comparison = Order.Compare;
 Algorithm.SortWith(Other.Spans.Of(values), comparison); return 42;
} }
'''
        project = write_project(root / 'counterfeit-span', counterfeit, args)
        code, output = common.run([args.compiler.resolve(), '--check-project-method-bodies',
                                   project, '--no-error-log'], repo)
        if code == 0 or "type arguments for generic method 'SortWith' cannot be inferred" not in output:
            failures.append('counterfeit Span: expected inference rejection')
    after = {str(path.resolve()): common.digest(path) for path in inputs}
    print('INPUT_SHA256_AFTER', after, flush=True)
    if before != after:
        failures.append('an input changed during the test')
    if failures:
        print('FAIL:', '; '.join(failures), file=sys.stderr, flush=True)
        return 1
    print('PASS: inferred and explicit SortWith semantics with binary SDK libraries', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
