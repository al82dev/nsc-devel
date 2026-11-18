"""Generic `delete` of an element whose type parameter is a dynamic array
(docs/fixes/delete-generico-de-arreglo.md).

EN: A generic container releases each element with one `delete items[i]` written over T, as the
    Core collections do. With T = byte[] that release is `delete[]`: the program compiles, and repeated
    add/remove/clear/destroy cycles run without heap corruption or a double release. The same container
    still works for T = int and for a class. A scalar `delete` written directly on a dynamic array
    outside a generic specialization is still rejected. Accepted cases run at O0..O3 (exit 42).
    This test does not measure leaks: Windows commits memory lazily, so a leak control did not fail.
ES: Un contenedor generico libera cada elemento con un `delete items[i]` escrito sobre T, como las
    colecciones de Core. Con T = byte[] esa liberacion es `delete[]`: el programa compila y los ciclos
    repetidos de agregar/quitar/limpiar/destruir corren sin corromper el heap ni liberar dos veces. El
    mismo contenedor sigue sirviendo para int y para una clase. Un `delete` escalar escrito directamente
    sobre un arreglo dinamico fuera de una especializacion generica se sigue rechazando. Esta prueba no
    mide fugas: Windows compromete memoria de forma perezosa y un control con fuga no fallo.
Usage: generic_array_element_delete.py <nsc.exe>
"""
from pathlib import Path
import subprocess
import sys
import tempfile

BOX = '''class Payload { public int Value; public Payload(int v) { Value = v; } }

class Box<T>
{
    private owned T[] items;
    private int count;

    public Box() { items = new T[8]; count = 0; }

    public void Add(owned T value) { items[count] = move(value); count = count + 1; }

    public int Count() { return count; }

    public void RemoveLast()
    {
        count = count - 1;
        delete items[count];
    }

    public void Clear()
    {
        int i = 0;
        while (i < count) { delete items[i]; i = i + 1; }
        count = 0;
    }

    ~Box()
    {
        int i = 0;
        while (i < count) { delete items[i]; i = i + 1; }
        delete[] items;
    }
}
'''

VALID = BOX + '''
class Program
{
    static int Arrays()
    {
        int round = 0;
        while (round < 400)
        {
            raii Box<byte[]> box = new Box<byte[]>();
            box.Add(new byte[4096]);
            box.Add(new byte[4096]);
            box.Add(new byte[4096]);
            box.RemoveLast();
            box.Clear();
            box.Add(new byte[4096]);
            round = round + 1;
        }
        return 0;
    }

    static int Values()
    {
        raii Box<int> numbers = new Box<int>();
        numbers.Add(40);
        numbers.Add(2);
        numbers.RemoveLast();
        raii Box<Payload> objects = new Box<Payload>();
        objects.Add(new Payload(2));
        objects.Clear();
        objects.Add(new Payload(2));
        return numbers.Count() + objects.Count() == 2 ? 0 : 1;
    }

    public static int Main() { return Arrays() + Values() == 0 ? 42 : 1; }
}
'''

SCALAR_OUTSIDE_GENERIC = '''class Program
{
    public static int Main()
    {
        owned byte[] data = new byte[4];
        delete data;
        return 42;
    }
}
'''


def run(command, cwd):
    return subprocess.run([str(part) for part in command], cwd=cwd,
                          capture_output=True, text=True, errors='replace')


def project(root, source):
    (root / 'Program.ns').write_text(source, encoding='utf-8')
    path = root / 'Case.nsproj'
    path.write_text('<Project><PropertyGroup><Name>Case</Name><OutputType>Exe</OutputType>'
                    '<Language>3</Language></PropertyGroup><ItemGroup><Compile Include="Program.ns" />'
                    '</ItemGroup></Project>', encoding='utf-8')
    return path


def main():
    compiler = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix='nsc-generic-array-delete-') as temporary:
        root = Path(temporary)
        case = project(root, SCALAR_OUTSIDE_GENERIC)
        result = run([compiler, '--check-project-method-bodies', case], root)
        output = result.stdout + result.stderr
        assert result.returncode != 0 and 'released with delete[]' in output, output
        print('scalar-delete-outside-generic rejected', flush=True)
        case = project(root, VALID)
        for level in ('O0', 'O1', 'O2', 'O3'):
            executable = root / ('case-' + level + '.exe')
            result = run([compiler, '--emit-project-oop-exe', case, '-' + level, '-o', executable,
                          '--no-artifact-cache'], root)
            assert result.returncode == 0, (level, result.stdout + result.stderr)
            result = run([executable], root)
            assert result.returncode == 42, (level, result.returncode)
            print('generic-array-delete', level, 'exit=42', flush=True)


if __name__ == '__main__':
    main()
