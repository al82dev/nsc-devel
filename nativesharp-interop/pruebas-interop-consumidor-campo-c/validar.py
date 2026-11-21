"""Consumidor separado de una biblioteca con un campo privado de tipo struct C.

El productor declara `private state s;` (struct C de su header) en una clase que hereda de Object. El consumidor
solo referencia la biblioteca y NativeSharp Core. Fallaba con "cannot prepare scoped C/C++ type names ... namespace
or alias requires unsupported resolution": el tipo C del productor activaba la preparacion de nombres C en el
consumidor, y la fuente de API publica reconstruida desde la metadata declara la base como
`using __NscMetadataBase0 = NativeSharpCore::NativeSharp.Core.Object;`, que el inventario lexico no reconocia.
Con un campo `int` (control) no hay tipo C y esa preparacion no se ejecuta.

Casos, O0-O3: campo struct C y control `int`; productor y consumidor compilados por separado; el consumidor ejecuta.

Uso: python validar.py --nsc <nsc.exe> --clang-root <raiz Clang> --core-root <dir con O0..O3\\NativeSharpCore.lib>
     [--nslink <nslink.exe>]
"""
import argparse
import os
import subprocess
import tempfile
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
parser.add_argument('--core-root', required=True)
parser.add_argument('--nslink')
args = parser.parse_args()
root = Path(tempfile.mkdtemp(prefix='nsc-consumidor-campo-c-', dir='A:/temp'))
env = dict(os.environ, TEMP=str(root), TMP=str(root))
if args.nslink:
    env['NSC_LINKER'] = args.nslink
print('Evidencia:', root, flush=True)


def run(label, command, cwd):
    result = subprocess.run(command, cwd=cwd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, errors='replace')
    (root / (label + '.log')).write_text('COMMAND: ' + repr(command) + '\nEXIT: ' + str(result.returncode) + '\n' +
                                         result.stdout, encoding='utf-8')
    if result.returncode and not label.endswith('-run'):
        print(result.stdout[-2000:], flush=True)
        raise SystemExit(label + ': fallo; evidencia: ' + str(root))
    return result


def core(opt):
    folder = Path(args.core_root) / f'O{opt}'
    return (f'<LibraryReference Include="{folder / "NativeSharpCore.lib"}" '
            f'Metadata="{folder / "NativeSharpCore.nsmeta"}" />')


FIELDS = {
    'struct-c': ('state', 's.value = v; s.extra = 1; return s.value + s.extra;'),
    'control-int': ('int', 's = v + 1; return s;'),
}
for opt in range(4):
    for name, (field, body) in FIELDS.items():
        case = root / f'O{opt}' / name
        producer = case / 'producer'
        consumer = case / 'consumer'
        producer.mkdir(parents=True)
        consumer.mkdir(parents=True)
        (producer / 'api.h').write_text('struct state { int value; int extra; };\n', encoding='ascii')
        (producer / 'Producer.ns').write_text(
            'using NativeSharp.Core;\nnamespace Prod\n{\n    public class Holder\n    {\n'
            f'        private {field} s;\n        public int Use(int v) {{ {body} }}\n    }}\n}}\n', encoding='utf-8')
        (producer / 'Producer.nsproj').write_text(
            '<Project><PropertyGroup><Name>CampoCProducer</Name><OutputType>Library</OutputType></PropertyGroup>'
            '<ItemGroup><Compile Include="Producer.ns" />'
            '<CppLibraryReference Header="api.h" Language="C" Standard="c17" />' + core(opt) +
            '</ItemGroup></Project>', encoding='utf-8')
        run(f'O{opt}-{name}-producer', [args.nsc, '--emit-project-oop-lib', 'Producer.nsproj',
                                         '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache',
                                         '-O' + str(opt), '-o', 'Producer.lib'], producer)
        (consumer / 'Main.ns').write_text(
            'using NativeSharp.Core;\nusing Prod;\n\nint Main()\n{\n    var h = new Holder();\n    return h.Use(41);\n}\n',
            encoding='utf-8')
        (consumer / 'Consumer.nsproj').write_text(
            '<Project><PropertyGroup><Name>CampoCConsumer</Name></PropertyGroup><ItemGroup>'
            '<Compile Include="Main.ns" /><LibraryReference Include="../producer/Producer.lib" />' + core(opt) +
            '</ItemGroup></Project>', encoding='utf-8')
        run(f'O{opt}-{name}-consumer', [args.nsc, '--emit-project-oop-exe', 'Consumer.nsproj',
                                         '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache',
                                         '-O' + str(opt), '-o', 'app.exe'], consumer)
        code = run(f'O{opt}-{name}-run', [str(consumer / 'app.exe')], consumer).returncode
        if code != 42:
            raise SystemExit(f'O{opt} {name}: devolvio {code}, se esperaba 42; evidencia: {root}')
        print(f'O{opt} {name}: productor y consumidor compilan; el consumidor devuelve 42', flush=True)
print('OK: consumidor de un campo privado struct C y su control int, O0-O3; evidencia:', root)
