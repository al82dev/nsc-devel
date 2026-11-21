"""Alias de tipo NativeSharp en un proyecto que importa un header C.

`using BufferUsageType = Demo.Saru.Framework.BufferUsage;` (un enum NativeSharp) fallaba con
"cannot prepare scoped C/C++ type names ... namespace or alias requires unsupported resolution" en cuanto el
proyecto agregaba un header C: cppScopedTypeNames abortaba ante un alias cuyo destino no era namespace ni tipo
C/C++. Ahora ese alias queda a cargo del indice NativeSharp ordinario, como sin header.

Casos, O0-O3: alias de tipo con y sin header, alias de namespace con header, alias a un struct C junto a un alias
NativeSharp. En O0, un alias a un tipo inexistente se rechaza con y sin header con el mismo motivo.

Uso: python validar.py --nsc <nsc.exe> --clang-root <raiz Clang> [--nslink <nslink.exe>]
"""
import argparse
import os
import subprocess
import sys
import tempfile
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
parser.add_argument('--nslink')
args = parser.parse_args()
root = Path(tempfile.mkdtemp(prefix='nsc-alias-tipo-ns-', dir='A:/temp'))
env = dict(os.environ, TEMP=str(root), TMP=str(root))
if args.nslink:
    env['NSC_LINKER'] = args.nslink
print('Evidencia:', root, flush=True)

ENUMS = 'namespace Demo.Saru.Framework\n{\n    public enum BufferUsage { None = 0, Dynamic = 7 }\n}\n'
HEADER = 'struct point { int x; int y; };\n'
INVALID = 'using Missing = Demo.Saru.Framework.NoExiste;\nint Main()\n{\n    Missing m;\n    return 0;\n}\n'
CASES = {
    'alias-tipo-con-header': ('using BufferUsageType = Demo.Saru.Framework.BufferUsage;\n'
                              'int Main()\n{\n    BufferUsageType u = BufferUsageType.Dynamic;\n'
                              '    point p; p.x = 30; p.y = 5;\n    return (int)u + p.x + p.y;\n}\n', True),
    'alias-tipo-sin-header': ('using BufferUsageType = Demo.Saru.Framework.BufferUsage;\n'
                              'int Main()\n{\n    BufferUsageType u = BufferUsageType.Dynamic;\n    return (int)u + 35;\n}\n',
                              False),
    'alias-namespace-con-header': ('using Fw = Demo.Saru.Framework;\n'
                                   'int Main()\n{\n    Fw.BufferUsage u = Fw.BufferUsage.Dynamic;\n'
                                   '    point p; p.x = 35;\n    return (int)u + p.x;\n}\n', True),
    'alias-c-y-ns-con-header': ('using Pt = point;\nusing BufferUsageType = Demo.Saru.Framework.BufferUsage;\n'
                                'int Main()\n{\n    Pt p; p.x = 35;\n    return (int)BufferUsageType.Dynamic + p.x;\n}\n',
                                True),
}


def run(label, command, cwd):
    result = subprocess.run(command, cwd=cwd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, errors='replace')
    (root / (label + '.log')).write_text('COMMAND: ' + repr(command) + '\nEXIT: ' + str(result.returncode) + '\n' +
                                         result.stdout, encoding='utf-8')
    return result


def project(case, main, header):
    case.mkdir(parents=True)
    (case / 'Enums.ns').write_text(ENUMS, encoding='utf-8')
    (case / 'Main.ns').write_text(main, encoding='utf-8')
    (case / 'api.h').write_text(HEADER, encoding='ascii')
    reference = '<CppLibraryReference Header="api.h" Language="C" Standard="c17" />' if header else ''
    (case / 'App.nsproj').write_text(
        '<Project><PropertyGroup><Name>AliasTipoNs</Name></PropertyGroup><ItemGroup><Compile Include="Enums.ns" />'
        '<Compile Include="Main.ns" />' + reference + '</ItemGroup></Project>', encoding='utf-8')


def nsc(label, case, opt):
    return run(label, [args.nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + args.clang_root,
                       '--no-artifact-cache', '-O' + str(opt), '-o', 'app.exe'], case)


for opt in range(4):
    for name, (main, header) in CASES.items():
        case = root / f'O{opt}' / name
        project(case, main, header)
        build = nsc(f'O{opt}-{name}-nsc', case, opt)
        if build.returncode:
            print(build.stdout[-2000:], flush=True)
            raise SystemExit(f'O{opt} {name}: nsc fallo; evidencia: {root}')
        code = run(f'O{opt}-{name}-run', [str(case / 'app.exe')], case).returncode
        if code != 42:
            raise SystemExit(f'O{opt} {name}: devolvio {code}, se esperaba 42; evidencia: {root}')
        print(f'O{opt} {name}: 42', flush=True)

reasons = []
for header in (True, False):
    name = 'invalido-' + ('con' if header else 'sin') + '-header'
    case = root / name
    project(case, INVALID, header)
    output = nsc(name, case, 0)
    if output.returncode == 0 or 'cannot prepare scoped' in output.stdout:
        print(output.stdout[-2000:], flush=True)
        raise SystemExit(f'{name}: el alias invalido no se rechazo por su causa; evidencia: {root}')
    expected = "using alias target 'Demo.Saru.Framework.NoExiste' does not name a visible namespace or type"
    if expected not in output.stdout:
        print(output.stdout[-2000:], flush=True)
        raise SystemExit(f'{name}: falta el diagnostico del alias; evidencia: {root}')
    print(f'{name}: rechazado: {expected}', flush=True)
print('OK: alias de tipo NativeSharp con y sin header C, O0-O3; evidencia:', root)
