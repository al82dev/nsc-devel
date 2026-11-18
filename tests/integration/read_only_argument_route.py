"""A borrowed argument the callee only reads is not route-exposed
(docs/fixes/argumento-solo-leido-no-expuesto.md).

EN: A tracked object passed together with another argument (`Contains(modes, candidate)`) used to be
    treated as possibly stored into it, even when the callee only reads a field of it. Destroying the
    object on the path that did not keep it then made a later `modes.Add(move(...))` "cannot prove that
    the referenced object is still alive". The callee body now proves the parameter is only read (plain
    field reads, `==`/`!=`), so the valid program compiles and runs at O0..O3 (exit 42). A callee that
    really stores the parameter (`holder.Slots[0] = mode`) is still rejected.
ES: Un objeto rastreado pasado junto con otro argumento (`Contains(modes, candidate)`) se trataba como
    posiblemente guardado en el, aunque el callee solo lee un campo suyo. Destruir el objeto en el camino
    que no lo conservo hacia que un `modes.Add(move(...))` posterior diera "cannot prove that the
    referenced object is still alive". El cuerpo del callee ahora demuestra que el parametro solo se lee
    (lecturas simples de campos, `==`/`!=`), asi que el programa valido compila y corre en O0..O3
    (sale 42). Un callee que de verdad guarda el parametro (`holder.Slots[0] = mode`) se sigue rechazando.
Usage: read_only_argument_route.py <nsc.exe>
"""
from pathlib import Path
import subprocess
import sys
import tempfile

VALID = '''class Mode
{
    public int Width;
    public Mode(int width) { Width = width; }
}

class Modes
{
    private owned Mode[] items;
    private int count;
    public Modes() { items = new Mode[2]; count = 0; }
    ~Modes() { int i = 0; while (i < count) { delete items[i]; i = i + 1; } delete[] items; }
    public int Count() { return count; }
    public Mode At(int index) { return items[index]; }
    public void Add(owned Mode mode)
    {
        if (count == items.Length)
        {
            owned Mode[] grown = new Mode[count * 2];
            for (int i = 0; i < count; i++) { grown[i] = move(items[i]); }
            delete[] items;
            items = move(grown);
        }
        items[count] = move(mode);
        count = count + 1;
    }
}

class Program
{
    static bool Contains(Modes modes, Mode mode)
    {
        if (mode == null) { return false; }
        for (int i = 0; i < modes.Count(); i++)
        {
            if (modes.At(i).Width == mode.Width) { return true; }
        }
        return false;
    }

    public static int Main()
    {
        raii Modes modes = new Modes();
        int index = 0;
        while (true)
        {
            if (index >= 12) { break; }
            index = index + 1;
            if (index % 3 == 0) { continue; }
            raii Mode candidate = new Mode(index % 5);
            if (!Contains(modes, candidate))
            {
                modes.Add(move(candidate));
            }
        }
        return modes.Count() == 5 ? 42 : modes.Count();
    }
}
'''

STORES = '''class Mode
{
    public int Width;
    public Mode(int width) { Width = width; }
}

class Holder
{
    public Mode[] Slots;
    public Holder() { Slots = new Mode[4]; }
    ~Holder() { delete[] Slots; }
    public int First() { return Slots[0].Width; }
}

class Program
{
    static bool Remember(Holder holder, Mode mode)
    {
        holder.Slots[0] = mode;
        return false;
    }

    public static int Main()
    {
        raii Holder holder = new Holder();
        int total = 0;
        int index = 0;
        while (index < 3)
        {
            index = index + 1;
            raii Mode candidate = new Mode(index);
            if (!Remember(holder, candidate)) { total = total + 1; }
            total = total + holder.First();
        }
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
    with tempfile.TemporaryDirectory(prefix='nsc-read-only-argument-') as temporary:
        root = Path(temporary)
        case = project(root, STORES)
        result = run([compiler, '--emit-project-oop-exe', case, '-O0', '-o', root / 'stores.exe',
                      '--no-artifact-cache'], root)
        output = result.stdout + result.stderr
        assert result.returncode != 0 and DIAGNOSTIC in output, output
        print('stored-argument rejected', flush=True)
        case = project(root, VALID)
        for level in ('O0', 'O1', 'O2', 'O3'):
            executable = root / ('case-' + level + '.exe')
            result = run([compiler, '--emit-project-oop-exe', case, '-' + level, '-o', executable,
                          '--no-artifact-cache'], root)
            assert result.returncode == 0, (level, result.stdout + result.stderr)
            result = run([executable], root)
            assert result.returncode == 42, (level, result.returncode)
            print('read-only-argument', level, 'exit=42', flush=True)


if __name__ == '__main__':
    main()
