"""`nsc --api-diff` y los calificadores de parametros de delegados.

El payload de metadata escribe los calificadores de puntero solo si algun valor de la biblioteca no es cero. Un
delegado se leia entonces con una lista vacia (biblioteca sin ningun `const T*`) o con un cero por parametro (la
misma biblioteca con algun `const T*` en otra declaracion, p. ej. por un header C), y `--api-diff` comparaba esa
lista cruda: la misma declaracion de delegado aparecia como cambio source-breaking.

Versiones de la biblioteca (O0-O3):
  v1         delegado `Reader(int* p)` y un generico, sin ningun `const T*`.
  v2-const   v1 + un metodo nuevo con `int const*`: cambio aditivo, el delegado no cambia.
  v2-header  v1 + un header C con un `const int*`: el delegado no cambia.
  v3-real    el parametro del delegado pasa a `int const*`: cambio real, debe detectarse.

Uso: python validar.py --nsc <nsc.exe> --clang-root <raiz Clang> [--nslink <nslink.exe>]
"""
import argparse
import os
import subprocess
import tempfile
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
parser.add_argument('--nslink')
args = parser.parse_args()
root = Path(tempfile.mkdtemp(prefix='nsc-api-diff-delegados-', dir='A:/temp'))
env = dict(os.environ, TEMP=str(root), TMP=str(root))
if args.nslink:
    env['NSC_LINKER'] = args.nslink
print('Evidencia:', root, flush=True)

BASE = '''namespace Lib
{
    public delegate int Reader(int* p);
    public delegate int Combine(int a, int b);
    public class Tools
    {
        public static T Pick<T>(T first, T second) { return first; }
        public static int Read(Reader reader, int* p) { return reader(p); }
EXTRA
    }
}
'''
VERSIONS = {
    'v1': (BASE.replace('EXTRA', ''), False),
    'v2-const': (BASE.replace('EXTRA', '        public static int ReadConst(int const* p) { return *p; }'), False),
    'v2-header': (BASE.replace('EXTRA', ''), True),
    'v3-real': (BASE.replace('Reader(int* p)', 'Reader(int const* p)').replace('EXTRA', ''), False),
}


def run(label, command, cwd):
    result = subprocess.run(command, cwd=cwd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, errors='replace')
    (root / (label + '.log')).write_text('COMMAND: ' + repr(command) + '\nEXIT: ' + str(result.returncode) + '\n' +
                                         result.stdout, encoding='utf-8')
    return result


for opt in range(4):
    metadata = {}
    for name, (source, header) in VERSIONS.items():
        case = root / f'O{opt}' / name
        case.mkdir(parents=True)
        (case / 'Lib.ns').write_text(source, encoding='utf-8')
        (case / 'api.h').write_text('int c_read(const int* p);\nstruct c_view { const int* data; };\n',
                                    encoding='ascii')
        reference = '<CppLibraryReference Header="api.h" Language="C" Standard="c17" />' if header else ''
        (case / 'Lib.nsproj').write_text(
            '<Project><PropertyGroup><Name>ApiDiffLib</Name><OutputType>Library</OutputType></PropertyGroup>'
            '<ItemGroup><Compile Include="Lib.ns" />' + reference + '</ItemGroup></Project>', encoding='utf-8')
        build = run(f'O{opt}-{name}', [args.nsc, '--emit-project-oop-lib', 'Lib.nsproj',
                                        '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache',
                                        '-O' + str(opt), '-o', 'ApiDiffLib.lib'], case)
        if build.returncode:
            print(build.stdout[-2000:], flush=True)
            raise SystemExit(f'O{opt} {name}: nsc fallo; evidencia: {root}')
        metadata[name] = case / 'ApiDiffLib.nsmeta'
    for name in ('v2-const', 'v2-header', 'v3-real'):
        diff = run(f'O{opt}-diff-{name}', [args.nsc, '--api-diff', str(metadata['v1']), str(metadata[name])], root)
        text = diff.stdout
        delegate_changed = any('[source-breaking]' in line and 'Reader' in line for line in text.splitlines())
        untouched_changed = any('Combine' in line for line in text.splitlines())
        compatible = 'source-compatible: yes' in text
        if name == 'v3-real':
            if compatible or not delegate_changed or untouched_changed:
                print(text[-2000:], flush=True)
                raise SystemExit(f'O{opt} {name}: no se detecto el cambio real del delegado; evidencia: {root}')
            print(f'O{opt} {name}: cambio real del delegado detectado (source-breaking)', flush=True)
        else:
            if not compatible or delegate_changed or untouched_changed:
                print(text[-2000:], flush=True)
                raise SystemExit(f'O{opt} {name}: diferencia espuria; evidencia: {root}')
            print(f'O{opt} {name}: source-compatible: yes', flush=True)
print('OK: api-diff de delegados sin diferencias de representacion y con cambios reales, O0-O3; evidencia:', root)
