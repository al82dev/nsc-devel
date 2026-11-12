"""Check the original SortWith example against separately built SDK libraries."""

import argparse
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time
from xml.sax.saxutils import quoteattr


def run(command, cwd):
    """Run a directed command with a bounded wait and observable progress."""
    arguments = [str(value) for value in command]
    print('COMMAND', subprocess.list2cmdline(arguments), 'CWD', cwd, flush=True)
    process = subprocess.Popen(arguments, cwd=cwd, stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT, text=True, errors='replace')
    deadline = time.monotonic() + 180
    while True:
        try:
            output, _ = process.communicate(timeout=min(25, max(0, deadline - time.monotonic())))
            break
        except subprocess.TimeoutExpired:
            if time.monotonic() >= deadline:
                process.kill()
                output, _ = process.communicate()
                print('TIMEOUT after 180 seconds', output, flush=True)
                return None, output
            print('RUNNING', arguments[0], flush=True)
    print('EXIT', process.returncode, output, flush=True)
    return process.returncode, output


def digest(path):
    """Return the SHA-256 of an input so changes invalidate the observation."""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def arguments():
    """Require explicit producer artifacts, with optional executable ASan mode."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--original', type=Path, required=True)
    parser.add_argument('--core-library', type=Path, required=True)
    parser.add_argument('--core-metadata', type=Path, required=True)
    parser.add_argument('--collections-library', type=Path, required=True)
    parser.add_argument('--collections-metadata', type=Path, required=True)
    parser.add_argument('--temp-root', type=Path, required=True)
    parser.add_argument('--mode', choices=('normal', 'asan', 'both'), required=True)
    parser.add_argument('--asan-clang', type=Path)
    parser.add_argument('--asan-runtime', type=Path)
    args = parser.parse_args()
    if args.mode in ('asan', 'both') and not (args.asan_clang and args.asan_runtime):
        parser.error('--mode asan/both requires --asan-clang and --asan-runtime')
    if args.mode == 'normal' and (args.asan_clang or args.asan_runtime):
        parser.error('--asan-clang and --asan-runtime require --mode asan/both')
    return args


def create_project(root, args):
    """Copy the reproduction unchanged and bind both libraries by binary metadata."""
    shutil.copyfile(args.original, root / 'Program.ns')
    references = ((args.core_library, args.core_metadata),
                  (args.collections_library, args.collections_metadata))
    text = '<Project><PropertyGroup><Name>SortWithBinaryOriginal</Name><OutputType>Exe</OutputType>'
    text += '</PropertyGroup><ItemGroup><Compile Include="Program.ns" />'
    for library, metadata in references:
        text += '<LibraryReference Include=' + quoteattr(library.resolve().as_posix())
        text += ' Metadata=' + quoteattr(metadata.resolve().as_posix()) + ' />'
    text += '</ItemGroup></Project>'
    project = root / 'Consumer.nsproj'
    project.write_text(text, encoding='utf-8')
    return project


def main():
    """Require the original source to compile and execute in each selected mode."""
    args = arguments()
    repo = Path(__file__).resolve().parents[2]
    paths = [args.compiler, args.original, args.core_library, args.core_metadata,
             args.collections_library, args.collections_metadata]
    if args.asan_clang:
        paths.extend([args.asan_clang, args.asan_runtime])
    if not args.temp_root.is_dir() or any(not path.is_file() for path in paths):
        print('ERROR: a required input is missing or --temp-root is not a directory', file=sys.stderr)
        return 2
    before = {str(path.resolve()): digest(path) for path in paths}
    print('INPUT_SHA256_BEFORE', before, flush=True)
    print('MODE', args.mode, 'ASAN_OPTIONS', os.environ.get('ASAN_OPTIONS', '<not set>'), flush=True)
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-sortwith-binary-', dir=args.temp_root) as temporary:
        root = Path(temporary)
        project = create_project(root, args)
        if digest(root / 'Program.ns') != digest(args.original):
            print('ERROR: reproduction copy differs from the original', file=sys.stderr)
            return 1
        modes = ('normal', 'asan') if args.mode == 'both' else (args.mode,)
        for mode in modes:
            for level in ('O0', 'O2'):
                executable = root / (mode + '-' + level + '.exe')
                command = [args.compiler.resolve(), '--emit-project-oop-exe', project, '-' + level,
                           '-o', executable, '--no-artifact-cache', '--no-error-log']
                if mode == 'asan':
                    command += ['--sanitize-address', '--asan-clang=' + str(args.asan_clang.resolve()),
                                '--asan-runtime=' + str(args.asan_runtime.resolve())]
                code, output = run(command, repo)
                if code != 0:
                    failures.append(mode + ' ' + level + ': compilation failed')
                    continue
                code, output = run([executable], repo)
                if code != 0 or 'ERROR: AddressSanitizer' in output:
                    failures.append(mode + ' ' + level + ': execution failed')
        if digest(root / 'Program.ns') != before[str(args.original.resolve())]:
            failures.append('the copied reproduction changed during the test')
    after = {str(path.resolve()): digest(path) for path in paths}
    print('INPUT_SHA256_AFTER', after, flush=True)
    if before != after:
        failures.append('an input changed during the test')
    if failures:
        print('FAIL:', '; '.join(failures), file=sys.stderr, flush=True)
        return 1
    print('PASS: supplied SortWith source with binary SDK libraries', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
