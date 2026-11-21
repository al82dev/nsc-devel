"""Metodos genericos virtuales de una clase precompilada llamados desde un consumidor separado.

El consumidor asignaba a cada especializacion de un metodo generico virtual (`Buffer.SetData<int>`) un slot nuevo al
final de la vtable de la clase importada (slots 3 y 4), pero esa vtable la emite el productor con los slots que el
conocia (3: ToString, Equals, GetHashCode). La llamada leia fuera de la tabla y saltaba a una direccion invalida
(0xC0000005), incluso con null y longitud cero, porque el despacho ocurre antes de entrar al cuerpo. Ahora una
especializacion virtual de un metodo generico declarado en una clase precompilada no recibe slot y se llama
directamente; es el mismo destino que el slot tiene en un solo proyecto, donde ningun override de una especializacion
generica llega a registrarse.

Casos, O0-O3, en un solo proyecto, con productor estatico y consumidor separados, y con productor DLL:
  - SetData<T> de 1, 3 y 5 argumentos con datos, null y longitud cero, y GetData<T> virtual;
  - una sobrecarga generica no virtual del mismo nombre;
  - controles de despacho virtual que no cambian: un virtual no generico redefinido en una subclase del productor
    y en una subclase local del consumidor, llamados por una referencia base;
  - SetData<T> sobre la subclase local del consumidor y sobre la subclase del productor (referencia base).

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
root = Path(tempfile.mkdtemp(prefix='nsc-genericos-virtuales-', dir='A:/temp'))
env = dict(os.environ, TEMP=str(root), TMP=str(root))
if args.nslink:
    env['NSC_LINKER'] = args.nslink
print('Evidencia:', root, flush=True)

LIBRARY = '''using NativeSharp.Core;
namespace Prod
{
    public class Buffer
    {
        private int writes;
        private int reads;
        public int Writes() { return writes; }
        public int Reads() { return reads; }
        public virtual int Kind() { return 1; }
        public void SetData<T>(T[] data)
        {
            if (data == null) { return; }
            SetData<T>(data, 0, data.Length);
        }
        public virtual void SetData<T>(T[] data, int startIndex, int elementCount)
        {
            SetData<T>(0, data, startIndex, elementCount, 7);
        }
        public virtual void SetData<T>(int offsetInBytes, T[] data, int startIndex, int elementCount, int options)
        {
            if (data == null && elementCount != 0) { return; }
            writes = writes + 1000 * options + elementCount * (int)sizeof(T) + offsetInBytes;
        }
        public virtual int GetData<T>(T[] data) { reads = reads + 1; return data == null ? -1 : data.Length; }
        public int SetData<T>(T value, int times) { writes = writes + times; return writes; }
    }
    public class Derived : Buffer
    {
        public override int Kind() { return 2; }
    }
}
'''
CONSUMER = '''using NativeSharp.Core;
using Prod;

public class Local : Buffer
{
    public override int Kind() { return 3; }
}

int Main()
{
    var buffer = new Buffer();
    int[] data = new int[3];
    buffer.SetData<int>(null, 0, 0);
    if (buffer.Writes() != 7000) return 1;
    buffer.SetData<int>(data, 0, 0);
    buffer.SetData<int>(data, 0, 3);
    if (buffer.Writes() != 21012) return 2;
    buffer.SetData<int>(null);
    buffer.SetData<int>(data);
    buffer.SetData<byte>(5, new byte[2], 0, 2, 1);
    if (buffer.Writes() != 29031) return 3;
    if (buffer.GetData<int>(data) != 3 || buffer.GetData<byte>(null) != -1 || buffer.Reads() != 2) return 4;
    if (buffer.SetData<int>(9, 4) != 29035) return 5;
    Buffer derived = new Derived();
    Buffer local = new Local();
    if (buffer.Kind() != 1 || derived.Kind() != 2 || local.Kind() != 3) return 6;
    derived.SetData<int>(data, 0, 1);
    local.SetData<int>(data, 0, 2);
    if (derived.Writes() != 7004 || local.Writes() != 7008) return 7;
    return 42;
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


def separated(case, opt, output_type, name):
    producer = case / 'productor'
    consumer = case / 'consumidor'
    producer.mkdir(parents=True)
    consumer.mkdir(parents=True)
    (producer / 'Lib.ns').write_text(LIBRARY, encoding='utf-8')
    (producer / 'Lib.nsproj').write_text(f'<Project><PropertyGroup><Name>{name}</Name><OutputType>{output_type}'
                                         '</OutputType></PropertyGroup><ItemGroup><Compile Include="Lib.ns" />' +
                                         core(opt) + '</ItemGroup></Project>', encoding='utf-8')
    dll = output_type == 'SharedLibrary'
    checked(f'O{opt}-{case.name}-productor',
            [args.nsc, '--emit-project-oop-dll' if dll else '--emit-project-oop-lib', 'Lib.nsproj',
             '--no-artifact-cache', '-O' + str(opt), '-o', name + ('.dll' if dll else '.lib')], producer)
    runtime = f' Runtime="../productor/{name}.dll"' if dll else ''
    (consumer / 'Main.ns').write_text(CONSUMER, encoding='utf-8')
    (consumer / 'App.nsproj').write_text('<Project><PropertyGroup><Name>GvApp</Name></PropertyGroup><ItemGroup>'
                                         '<Compile Include="Main.ns" />'
                                         f'<LibraryReference Include="../productor/{name}.lib"{runtime} />' +
                                         core(opt) + '</ItemGroup></Project>', encoding='utf-8')
    checked(f'O{opt}-{case.name}-consumidor', [args.nsc, '--emit-project-oop-exe', 'App.nsproj',
                                               '--no-artifact-cache', '-O' + str(opt), '-o', 'app.exe'], consumer)
    if dll:
        (consumer / (name + '.dll')).write_bytes((producer / (name + '.dll')).read_bytes())
    execute(f'O{opt}-{case.name}-run', consumer / 'app.exe')


for opt in range(4):
    case = root / f'O{opt}'
    same = case / 'mismo-proyecto'
    same.mkdir(parents=True)
    (same / 'Lib.ns').write_text(LIBRARY, encoding='utf-8')
    (same / 'Main.ns').write_text(CONSUMER, encoding='utf-8')
    (same / 'App.nsproj').write_text('<Project><PropertyGroup><Name>GvSame</Name></PropertyGroup><ItemGroup>'
                                     '<Compile Include="Lib.ns" /><Compile Include="Main.ns" />' + core(opt) +
                                     '</ItemGroup></Project>', encoding='utf-8')
    checked(f'O{opt}-mismo-proyecto', [args.nsc, '--emit-project-oop-exe', 'App.nsproj', '--no-artifact-cache',
                                       '-O' + str(opt), '-o', 'app.exe'], same)
    execute(f'O{opt}-mismo-proyecto-run', same / 'app.exe')
    separated(case / 'estatica', opt, 'Library', 'GvLib')
    separated(case / 'dll', opt, 'SharedLibrary', 'GvDll')
    print(f'O{opt}: mismo proyecto, consumidor de .lib y consumidor de DLL devuelven 42', flush=True)
print('OK: metodos genericos virtuales de clases precompiladas, O0-O3; evidencia:', root)
