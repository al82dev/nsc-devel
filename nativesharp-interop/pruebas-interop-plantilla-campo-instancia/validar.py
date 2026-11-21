"""Plantilla publica que llama un metodo de un tipo interno a traves de un campo privado de instancia.

`Buffer.GetData<T>` (publico) llama `store.Read(...)`, donde `store` es un campo privado de instancia de tipo
`Store` (interno). El consumidor de la biblioteca estatica instancia la plantilla y referencia
`Store.Read`. La validacion de referencias precompiladas publicaba, para las plantillas, los metodos de los tipos de
los campos ESTATICOS del dueno de la plantilla, pero no los de los campos de instancia, y rechazaba el simbolo
como "unpublished symbol" aunque estuviera definido en la .lib. Ahora se recogen los campos de ambos tipos.

Casos, O0-O3, en un solo proyecto y con productor y consumidor separados:
  - GetData<T>(T[]) y GetData<T>(int, T[], int, int) con datos, null y longitud cero;
  - control: un helper alcanzado por un campo estatico (ya funcionaba).
Y en O0:
  - control negativo: si el simbolo se renombra dentro de la .lib, la validacion lo sigue rechazando;
  - limite: con el productor como DLL el helper interno no se exporta y el consumidor se rechaza igual que antes.

Uso: python validar.py --nsc <nsc.exe> --core-root <dir con O0..O3\\NativeSharpCore.lib> [--nslink <nslink.exe>]
     [--llvm-objcopy <llvm-objcopy.exe>]
"""
import argparse
import os
import shutil
import subprocess
import tempfile
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--core-root', required=True)
parser.add_argument('--nslink')
parser.add_argument('--llvm-objcopy', default=shutil.which('llvm-objcopy'))
args = parser.parse_args()
root = Path(tempfile.mkdtemp(prefix='nsc-plantilla-campo-instancia-', dir='A:/temp'))
env = dict(os.environ, TEMP=str(root), TMP=str(root))
if args.nslink:
    env['NSC_LINKER'] = args.nslink
print('Evidencia:', root, flush=True)

LIBRARY = '''using NativeSharp.Core;
namespace Prod
{
    internal sealed class Store
    {
        public bool Read(int offsetInBytes, void* destination, int count)
        {
            byte* bytes = (byte*)destination;
            for (int i = 0; i < count; i++) { bytes[i] = (byte)(offsetInBytes + i + 1); }
            return true;
        }
    }
    internal sealed class Counter
    {
        private int total;
        public int Add(int value) { total = total + value; return total; }
    }
    public class Buffer
    {
        private owned Store store;
        private static Counter counter = new Counter();
        public Buffer() { store = new Store(); }
        public void GetData<T>(T[] data)
        {
            if (data == null) { return; }
            GetData<T>(0, data, 0, data.Length);
        }
        public void GetData<T>(int offsetInBytes, T[] data, int startIndex, int elementCount)
        {
            if (data == null || elementCount == 0) { return; }
            store.Read(offsetInBytes, (void*)&data[startIndex], elementCount * (int)sizeof(T));
        }
        public int Count<T>(T[] data) { return counter.Add(data.Length); }
    }
}
'''
CONSUMER = '''using NativeSharp.Core;
using Prod;

int Main()
{
    var buffer = new Buffer();
    int[] data = new int[2];
    buffer.GetData<int>(null);
    buffer.GetData<int>(0, null, 0, 0);
    buffer.GetData<int>(0, data, 0, 0);
    if (data[0] != 0 || data[1] != 0) return 1;
    buffer.GetData<int>(data);
    if (data[0] != 0x04030201 || data[1] != 0x08070605) return 2;
    byte[] bytes = new byte[4];
    buffer.GetData<byte>(10, bytes, 1, 2);
    if (bytes[0] != 0 || bytes[1] != 11 || bytes[2] != 12 || bytes[3] != 0) return 3;
    if (buffer.Count<int>(data) != 2 || buffer.Count<byte>(bytes) != 6) return 4;
    return 42;
}
'''
UNPUBLISHED = 'unpublished symbol'
READ_SYMBOL = '__nsc_method_{module}$3A$3AProd$2EStore__Read__builtin$3Aint__builtin$3Avoid$2A__builtin$3Aint'


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


def rejected(label, command, cwd):
    result = run(label, command, cwd)
    if result.returncode == 0 or UNPUBLISHED not in result.stdout:
        print(result.stdout[-2500:], flush=True)
        raise SystemExit(label + ': se esperaba el rechazo por simbolo no publicado; evidencia: ' + str(root))


def producer(folder, opt, output_type, name):
    folder.mkdir(parents=True)
    (folder / 'Lib.ns').write_text(LIBRARY, encoding='utf-8')
    (folder / 'Lib.nsproj').write_text(f'<Project><PropertyGroup><Name>{name}</Name><OutputType>{output_type}'
                                       '</OutputType></PropertyGroup><ItemGroup><Compile Include="Lib.ns" />' +
                                       core(opt) + '</ItemGroup></Project>', encoding='utf-8')
    if output_type == 'SharedLibrary':
        checked(f'O{opt}-{folder.name}', [args.nsc, '--emit-project-oop-dll', 'Lib.nsproj', '--no-artifact-cache',
                                          '-O' + str(opt), '-o', name + '.dll'], folder)
    else:
        checked(f'O{opt}-{folder.name}', [args.nsc, '--emit-project-oop-lib', 'Lib.nsproj', '--no-artifact-cache',
                                          '-O' + str(opt), '-o', name + '.lib'], folder)


def consumer(folder, opt, reference):
    folder.mkdir(parents=True)
    (folder / 'Main.ns').write_text(CONSUMER, encoding='utf-8')
    (folder / 'App.nsproj').write_text('<Project><PropertyGroup><Name>CampoApp</Name></PropertyGroup><ItemGroup>'
                                       '<Compile Include="Main.ns" />' + reference + core(opt) +
                                       '</ItemGroup></Project>', encoding='utf-8')
    return [args.nsc, '--emit-project-oop-exe', 'App.nsproj', '--no-artifact-cache', '-O' + str(opt), '-o', 'app.exe']


for opt in range(4):
    case = root / f'O{opt}'
    same = case / 'mismo-proyecto'
    same.mkdir(parents=True)
    (same / 'Lib.ns').write_text(LIBRARY, encoding='utf-8')
    (same / 'Main.ns').write_text(CONSUMER, encoding='utf-8')
    (same / 'App.nsproj').write_text('<Project><PropertyGroup><Name>CampoSame</Name></PropertyGroup><ItemGroup>'
                                     '<Compile Include="Lib.ns" /><Compile Include="Main.ns" />' + core(opt) +
                                     '</ItemGroup></Project>', encoding='utf-8')
    checked(f'O{opt}-mismo-proyecto', [args.nsc, '--emit-project-oop-exe', 'App.nsproj', '--no-artifact-cache',
                                       '-O' + str(opt), '-o', 'app.exe'], same)
    execute(f'O{opt}-mismo-proyecto-run', same / 'app.exe')
    producer(case / 'productor', opt, 'Library', 'CampoLib')
    command = consumer(case / 'consumidor', opt, '<LibraryReference Include="../productor/CampoLib.lib" />')
    checked(f'O{opt}-consumidor', command, case / 'consumidor')
    execute(f'O{opt}-consumidor-run', case / 'consumidor' / 'app.exe')
    print(f'O{opt}: mismo proyecto y consumidor separado devuelven 42', flush=True)

# Negative control: the helper renamed inside the archive is still rejected.
# Control negativo: el helper renombrado dentro del archivo se sigue rechazando.
if not args.llvm_objcopy:
    raise SystemExit('se necesita llvm-objcopy para el control negativo (--llvm-objcopy)')
case = root / 'simbolo-inexistente'
producer(case / 'productor', 0, 'Library', 'CampoLib')
checked('simbolo-inexistente-renombrar', [args.llvm_objcopy, '--redefine-sym',
                                          READ_SYMBOL.format(module='CampoLib') + '=__nsc_renamed_read',
                                          'CampoLib.lib'], case / 'productor')
command = consumer(case / 'consumidor', 0, '<LibraryReference Include="../productor/CampoLib.lib" />')
rejected('simbolo-inexistente-consumidor', command, case / 'consumidor')
print('simbolo inexistente: la validacion lo sigue rechazando', flush=True)

# Limit: a DLL does not export the internal helper, so the consumer is rejected as before.
# Limite: una DLL no exporta el helper interno; el consumidor se rechaza como antes.
case = root / 'dll'
producer(case / 'productor', 0, 'SharedLibrary', 'CampoDll')
command = consumer(case / 'consumidor', 0,
                   '<LibraryReference Include="../productor/CampoDll.lib" Runtime="../productor/CampoDll.dll" />')
rejected('dll-consumidor', command, case / 'consumidor')
print('dll: el helper interno no se exporta; se conserva el rechazo (limite documentado)', flush=True)
print('OK: helper alcanzado por campo de instancia desde una plantilla, O0-O3; evidencia:', root)
