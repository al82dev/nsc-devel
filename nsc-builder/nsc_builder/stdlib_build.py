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
    # EN: The artifacts of this compiler build stay below its package prefix, so two compiler builds never
    #     overwrite each other's libraries in the shared <libraries>/build folder.
    # ES: Los artefactos de esta construccion del compilador quedan bajo su prefijo de paquetes, asi dos
    #     construcciones del compilador no se pisan las bibliotecas en la carpeta compartida <libraries>/build.
    build_root = args.package_prefix.resolve() / 'nativesharp-libraries-build'
    base = ['python', '-B', str(libraries / 'nativesharp_libraries.py'), '--root', str(libraries),
            '--compiler', str(args.compiler.resolve()), '--build-root', str(build_root)]
    # EN: An older NativeSharp Libraries checkout has no --build-root and reads the folder as its command; say
    #     which checkout is outdated instead of that argparse error.
    # ES: Un checkout antiguo de NativeSharp Libraries no tiene --build-root y lee la carpeta como su comando; se
    #     dice que checkout esta desactualizado en lugar de ese error de argparse.
    probe = subprocess.run(['python', '-B', str(libraries / 'nativesharp_libraries.py'), '-h'], cwd=libraries,
                           env=env, check=False, capture_output=True, text=True)
    if probe.returncode != 0 or '--build-root' not in probe.stdout:
        print('stdlib_build: error: ' + str(libraries / 'nativesharp_libraries.py') + ' does not accept '
              '--build-root; update that NativeSharp Libraries checkout (41d76a9 or later)', flush=True)
        return 2
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
