"""An unknown call's own exposure does not taint the uses it is checked for
(docs/fixes/uso-antes-del-escape-de-la-llamada.md).

EN: A fresh `raii` receiver's first unsummarized call (`second.Size()`) was rejected with "cannot prove
    that the referenced object is still alive when this call uses a stored reference" because the call
    escaped the receiver before its uses were assessed: the receiver's field arrays then named every
    exposed object, including one destroyed earlier in another block. The uses are now assessed against
    what the receiver stored before the call, so the valid program compiles and runs at O0..O3
    (exit 42). A receiver that really stored the destroyed object in an earlier call
    (`holder.Keep(first)` then `holder.Use()`) is still rejected.
ES: La primera llamada sin resumen de un receptor `raii` nuevo (`second.Size()`) se rechazaba con
    "cannot prove that the referenced object is still alive when this call uses a stored reference"
    porque la llamada escapaba al receptor antes de evaluar sus usos: los arreglos de campo del receptor
    nombraban entonces todo objeto expuesto, incluido uno destruido antes en otro bloque. Ahora los usos
    se evaluan con lo que el receptor guardaba antes de la llamada; el programa valido compila y corre
    en O0..O3 (sale 42). Un receptor que de verdad guardo el objeto destruido en una llamada anterior
    (`holder.Keep(first)` y luego `holder.Use()`) se sigue rechazando.
Usage: unknown_call_uses_before_escape.py <nsc.exe>
"""
from pathlib import Path
import subprocess
import sys
import tempfile

VALID = '''class Text
{
    private int[] data;
    public Text(int length) { data = new int[length]; }
    ~Text() { delete[] data; }
    public virtual int Size() { return data.Length; }
}

class Program
{
    public static int Main()
    {
        int total = 0;
        if (total == 0)
        {
            raii Text first = new Text(18);
            total = total + first.Size();
        }
        raii Text second = new Text(24);
        total = total + second.Size();
        return total;
    }
}
'''

STORES = '''class Text
{
    private int[] data;
    public Text(int length) { data = new int[length]; }
    ~Text() { delete[] data; }
    public virtual int Size() { return data.Length; }
}

class Holder
{
    private Text[] kept;
    public Holder() { kept = new Text[1]; }
    ~Holder() { delete[] kept; }
    public virtual void Keep(Text text) { kept[0] = text; }
    public virtual int Use() { return kept[0].Size(); }
}

class Program
{
    public static int Main()
    {
        raii Holder holder = new Holder();
        int total = 0;
        if (total == 0)
        {
            raii Text first = new Text(18);
            holder.Keep(first);
        }
        total = total + holder.Use();
        return total;
    }
}
'''

DIAGNOSTIC = 'cannot prove that the referenced object is still alive'


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
    with tempfile.TemporaryDirectory(prefix='nsc-uses-before-escape-') as temporary:
        root = Path(temporary)
        case = project(root, STORES)
        result = run([compiler, '--emit-project-oop-exe', case, '-O0', '-o', root / 'stores.exe',
                      '--no-artifact-cache'], root)
        output = result.stdout + result.stderr
        assert result.returncode != 0 and DIAGNOSTIC in output, output
        print('stored-destroyed-object rejected', flush=True)
        case = project(root, VALID)
        for level in ('O0', 'O1', 'O2', 'O3'):
            executable = root / ('case-' + level + '.exe')
            result = run([compiler, '--emit-project-oop-exe', case, '-' + level, '-o', executable,
                          '--no-artifact-cache'], root)
            assert result.returncode == 0, (level, result.stdout + result.stderr)
            result = run([executable], root)
            assert result.returncode == 42, (level, result.returncode)
            print('uses-before-escape', level, 'exit=42', flush=True)


if __name__ == '__main__':
    main()
