"""Llamadas de instancia a metodos genericos sobrecargados o con nombres repetidos en otros tipos.

`x.M<T>(...)` con un receptor valor de un tipo no generico se resolvia solo por nombre y numero de argumentos entre
todas las plantillas visibles, porque el receptor se escribe como la variable. Fallaba con "has no visible Stage 44
generic method template" cuando el tipo del receptor tenia sobrecargas con el mismo numero de argumentos
(`DrawUserIndexedPrimitives<T>` con indices ushort/int/uint) o cuando otro tipo declaraba un metodo generico con el
mismo nombre y numero (`Texture2D.SetData<T>(T[])` y `VertexBuffer.SetData<T>(T[])`). Ahora, solo cuando esa
busqueda ya fallo, el tipo estatico del receptor elige el dueno.

Casos, O0-O3, en un solo proyecto y con productor y consumidor separados:
  - tres sobrecargas de igual aridad (ushort[]/int[]/uint[]) en el mismo tipo;
  - SetData<T>/GetData<T> con 1 y 3 parametros en dos tipos distintos;
  - controles que ya funcionaban: metodo generico de nombre unico y llamada estatica sobrecargada.
Y en O0 un metodo heredado sigue con el resultado previo (no se amplia a herencia).

Uso: python validar.py --nsc <nsc.exe> --core-root <dir con O0..O3\\NativeSharpCore.lib> [--nslink <nslink.exe>]
"""
import argparse
import os
import subprocess
import tempfile
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--core-root', required=True)
parser.add_argument('--nslink')
args = parser.parse_args()
root = Path(tempfile.mkdtemp(prefix='nsc-genericos-receptor-', dir='A:/temp'))
env = dict(os.environ, TEMP=str(root), TMP=str(root))
if args.nslink:
    env['NSC_LINKER'] = args.nslink
print('Evidencia:', root, flush=True)

LIBRARY = '''using NativeSharp.Core;
namespace Prod
{
    public class Texture
    {
        private int total;
        public void SetData<T>(T[] data) { total = total + data.Length; }
        public void SetData<T>(T[] data, int startIndex, int elementCount) { total = total + elementCount * 10; }
        public int GetData<T>(T[] data) { return total; }
    }
    public class VertexStore
    {
        private int total;
        public void SetData<T>(T[] data) { total = total + 100 * data.Length; }
        public void SetData<T>(T[] data, int startIndex, int elementCount) { total = total + elementCount * 1000; }
        public int GetData<T>(T[] data) { return total; }
    }
    public class Device
    {
        public int Draw<T>(T[] vertices, ushort[] indices) { return 1; }
        public int Draw<T>(T[] vertices, int[] indices) { return 2; }
        public int Draw<T>(T[] vertices, uint[] indices) { return 3; }
        public int Unique<T>(T[] values) { return values.Length; }
        public static int Pick<T>(T[] vertices, ushort[] indices) { return 4; }
        public static int Pick<T>(T[] vertices, int[] indices) { return 5; }
    }
}
'''
CONSUMER = '''using NativeSharp.Core;
using Prod;

int Main()
{
    int[] data = new int[3];
    ushort[] shortIndices = new ushort[1];
    int[] intIndices = new int[1];
    uint[] uintIndices = new uint[1];
    var texture = new Texture();
    VertexStore store = new VertexStore();
    var device = new Device();
    texture.SetData<int>(data);
    texture.SetData<int>(data, 0, 2);
    store.SetData<int>(data);
    store.SetData<int>(data, 0, 2);
    if (texture.GetData<int>(data) != 23) return 1;
    if (store.GetData<int>(data) != 2300) return 2;
    int draws = device.Draw<int>(data, shortIndices) * 100 + device.Draw<int>(data, intIndices) * 10 +
        device.Draw<int>(data, uintIndices);
    if (draws != 123) return 3;
    if (device.Unique<int>(data) != 3) return 4;
    if (Device.Pick<int>(data, shortIndices) != 4 || Device.Pick<int>(data, intIndices) != 5) return 5;
    return 42;
}
'''
INHERITED = '''namespace Prod
{
    public class Base
    {
        public int Set<T>(T[] data) { return 1; }
    }
    public class Derived : Base
    {
    }
    public class Other
    {
        public int Set<T>(T[] data) { return 2; }
    }
}
'''


def run(label, command, cwd):
    result = subprocess.run(command, cwd=cwd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, errors='replace')
    (root / (label + '.log')).write_text('COMMAND: ' + repr(command) + '\nEXIT: ' + str(result.returncode) + '\n' +
                                         result.stdout, encoding='utf-8')
    return result


def core(opt):
    folder = Path(args.core_root) / f'O{opt}'
    return (f'<LibraryReference Include="{folder / "NativeSharpCore.lib"}" '
            f'Metadata="{folder / "NativeSharpCore.nsmeta"}" />')


def checked(label, command, cwd):
    result = run(label, command, cwd)
    if result.returncode:
        print(result.stdout[-2500:], flush=True)
        raise SystemExit(label + ': fallo; evidencia: ' + str(root))


def execute(label, executable):
    code = run(label, [str(executable)], executable.parent).returncode
    if code != 42:
        raise SystemExit(f'{label}: devolvio {code}, se esperaba 42; evidencia: {root}')


for opt in range(4):
    case = root / f'O{opt}'
    same = case / 'mismo-proyecto'
    producer = case / 'productor'
    consumer = case / 'consumidor'
    for folder in (same, producer, consumer):
        folder.mkdir(parents=True)
    (same / 'Lib.ns').write_text(LIBRARY, encoding='utf-8')
    (same / 'Main.ns').write_text(CONSUMER, encoding='utf-8')
    (same / 'App.nsproj').write_text('<Project><PropertyGroup><Name>GenRecv</Name></PropertyGroup><ItemGroup>'
                                     '<Compile Include="Lib.ns" /><Compile Include="Main.ns" />' + core(opt) +
                                     '</ItemGroup></Project>', encoding='utf-8')
    checked(f'O{opt}-mismo-proyecto', [args.nsc, '--emit-project-oop-exe', 'App.nsproj', '--no-artifact-cache',
                                       '-O' + str(opt), '-o', 'app.exe'], same)
    execute(f'O{opt}-mismo-proyecto-run', same / 'app.exe')
    (producer / 'Lib.ns').write_text(LIBRARY, encoding='utf-8')
    (producer / 'Lib.nsproj').write_text('<Project><PropertyGroup><Name>GenRecvLib</Name><OutputType>Library'
                                         '</OutputType></PropertyGroup><ItemGroup><Compile Include="Lib.ns" />' +
                                         core(opt) + '</ItemGroup></Project>', encoding='utf-8')
    checked(f'O{opt}-productor', [args.nsc, '--emit-project-oop-lib', 'Lib.nsproj', '--no-artifact-cache',
                                  '-O' + str(opt), '-o', 'GenRecvLib.lib'], producer)
    (consumer / 'Main.ns').write_text(CONSUMER, encoding='utf-8')
    (consumer / 'App.nsproj').write_text('<Project><PropertyGroup><Name>GenRecvApp</Name></PropertyGroup><ItemGroup>'
                                         '<Compile Include="Main.ns" />'
                                         '<LibraryReference Include="../productor/GenRecvLib.lib" />' + core(opt) +
                                         '</ItemGroup></Project>', encoding='utf-8')
    checked(f'O{opt}-consumidor', [args.nsc, '--emit-project-oop-exe', 'App.nsproj', '--no-artifact-cache',
                                   '-O' + str(opt), '-o', 'app.exe'], consumer)
    execute(f'O{opt}-consumidor-run', consumer / 'app.exe')
    print(f'O{opt}: mismo proyecto y consumidor separado devuelven 42', flush=True)

# A method inherited from a base keeps the previous lexical result: still ambiguous with Other.Set.
# Un metodo heredado conserva el resultado lexico previo: sigue ambiguo con Other.Set.
case = root / 'heredado'
case.mkdir()
(case / 'Lib.ns').write_text(INHERITED, encoding='utf-8')
(case / 'Main.ns').write_text('using Prod;\nint Main()\n{\n    int[] data = new int[1];\n    var d = new Derived();\n'
                              '    return d.Set<int>(data);\n}\n', encoding='utf-8')
(case / 'App.nsproj').write_text('<Project><PropertyGroup><Name>GenRecvInh</Name></PropertyGroup><ItemGroup>'
                                 '<Compile Include="Lib.ns" /><Compile Include="Main.ns" /></ItemGroup></Project>',
                                 encoding='utf-8')
inherited = run('heredado', [args.nsc, '--emit-project-oop-exe', 'App.nsproj', '--no-artifact-cache', '-O0', '-o',
                             'app.exe'], case)
if inherited.returncode == 0 or 'Set<...>' not in inherited.stdout:
    print(inherited.stdout[-2000:], flush=True)
    raise SystemExit('heredado: el resultado cambio respecto del previo; evidencia: ' + str(root))
print('heredado: conserva el diagnostico previo (no se amplia a metodos heredados)', flush=True)
print('OK: metodos genericos por receptor valor, O0-O3; evidencia:', root)
