"""Item 18.16 (D12): find the installed Windows SDK and Visual C++ include directories for a NativeSharp project.

Prints the <WindowsSdkIncludeRoot> and <VcToolsIncludeRoot> properties to paste into a .nsproj PropertyGroup, or
writes them into a project with --write <project.nsproj>. Search order: the command line roots, the environment
variables WindowsSdkDir / VCToolsInstallDir, then the default installation folders.

Item 18.16 (D12): busca los directorios include del Windows SDK y de Visual C++ instalados para un proyecto
NativeSharp e imprime las propiedades para el .nsproj, o las escribe con --write.

Usage: python detect_windows_sdk.py [--sdk-root <Windows Kits\\10>] [--vc-root <VC\\Tools\\MSVC>]
                                    [--version 10.0.28000.0] [--write project.nsproj]
"""
import argparse
import os
import re
import sys
from pathlib import Path


def version_key(name):
    return tuple(int(part) for part in re.findall(r'\d+', name))


def find_sdk_include(roots, wanted):
    for root in roots:
        include = Path(root) / 'Include'
        if not include.is_dir():
            continue
        versions = sorted((p for p in include.iterdir() if (p / 'ucrt' / 'stdio.h').is_file()),
                          key=lambda p: version_key(p.name), reverse=True)
        for candidate in versions:
            if wanted is None or candidate.name == wanted:
                return candidate
    return None


def find_vc_include(roots):
    for root in roots:
        tools = Path(root)
        if not tools.is_dir():
            continue
        versions = sorted((p for p in tools.iterdir() if (p / 'include' / 'vcruntime.h').is_file()),
                          key=lambda p: version_key(p.name), reverse=True)
        if versions:
            return versions[0] / 'include'
        if (tools / 'include' / 'vcruntime.h').is_file():
            return tools / 'include'
    return None


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--sdk-root', action='append', default=[])
    parser.add_argument('--vc-root', action='append', default=[])
    parser.add_argument('--version')
    parser.add_argument('--write')
    args = parser.parse_args()
    sdk_roots = args.sdk_root + [os.environ.get('WindowsSdkDir', ''),
                                 r'C:\Program Files (x86)\Windows Kits\10']
    vc_roots = args.vc_root + [os.environ.get('VCToolsInstallDir', '')]
    for edition in ('Community', 'Professional', 'Enterprise', 'BuildTools'):
        for year in ('2026', '2022', '18'):
            vc_roots.append(rf'C:\Program Files\Microsoft Visual Studio\{year}\{edition}\VC\Tools\MSVC')
    sdk = find_sdk_include([r for r in sdk_roots if r], args.version)
    vc = find_vc_include([r for r in vc_roots if r])
    if sdk is None:
        print('Windows SDK not found' + (f' (version {args.version})' if args.version else ''), file=sys.stderr)
        return 1
    properties = f'<WindowsSdkIncludeRoot>{sdk}</WindowsSdkIncludeRoot>'
    if vc is not None:
        properties += f'<VcToolsIncludeRoot>{vc}</VcToolsIncludeRoot>'
    else:
        print('Visual C++ include directory (vcruntime.h) not found; the UCRT headers need it', file=sys.stderr)
    if args.write:
        project = Path(args.write)
        text = project.read_text(encoding='utf-8')
        text = re.sub(r'<WindowsSdkIncludeRoot>.*?</WindowsSdkIncludeRoot>|<VcToolsIncludeRoot>.*?</VcToolsIncludeRoot>',
                      '', text)
        if '<PropertyGroup>' not in text:
            print('the project has no <PropertyGroup>', file=sys.stderr)
            return 1
        project.write_text(text.replace('<PropertyGroup>', '<PropertyGroup>' + properties, 1), encoding='utf-8')
        print(f'{project}: {properties}')
    else:
        print(properties)
    return 0


if __name__ == '__main__':
    sys.exit(main())
