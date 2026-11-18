"""Build the consolidated standard libraries with the compiler just produced."""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import subprocess


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument('--libraries', type=Path, required=True)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--imports', type=Path, required=True)
    parser.add_argument('--linker', type=Path, required=True)
    parser.add_argument('--package-prefix', type=Path, required=True)
    parser.add_argument('--jobs', type=int, required=True)
    args = parser.parse_args()
    libraries = args.libraries.resolve()
    env = os.environ.copy()
    env['NSC_WINDOWS_RUNTIME_LIBRARY'] = str(args.runtime.resolve())
    env['NSC_WINDOWS_RUNTIME_IMPORT_LIBRARY_DIRECTORY'] = str(args.imports.resolve())
    env['NSC_LINKER'] = str(args.linker.resolve())
    base = ['python', '-B', str(libraries / 'nativesharp_libraries.py'), '--root', str(libraries),
            '--compiler', str(args.compiler.resolve())]
    packages = []
    for package in ('NativeSharp.Core', 'NativeSharp.Core.Collections.Generic', 'NativeSharp.Core.Threading',
                    'NativeSharp.Core.IO', 'NativeSharp.Core.Diagnostics'):
        packages.extend(['--package', package])
    command = base + ['build', '--all-levels', '-j', str(args.jobs)] + packages
    result = subprocess.run(command, cwd=libraries, env=env, check=False)
    if result.returncode:
        return result.returncode
    for level in ('O0', 'O1', 'O2', 'O3'):
        command = base + ['install', '--prefix', str(args.package_prefix.resolve()), '--opt', level] + packages
        result = subprocess.run(command, cwd=libraries, env=env, check=False)
        if result.returncode:
            return result.returncode
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
