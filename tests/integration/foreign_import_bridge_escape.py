"""Escape contract of a precompiled bridge to an imported C function
(docs/fixes/contrato-escape-importacion-c.md).

EN: A binding package turns each C import into a public NativeSharp method that only forwards to it.
    Passing the address of a local to that bridge from another package is accepted, as it is when the
    import is called directly; it is also accepted through a consumer wrapper. A bridge that also keeps
    the pointer in static storage is still rejected, and a direct import call stays accepted.
ES: Un paquete de bindings convierte cada importacion C en un metodo NativeSharp publico que solo le
    reenvia. Pasar la direccion de un local a ese puente desde otro paquete se acepta, como cuando se
    llama directo a la importacion; tambien a traves de un envoltorio del consumidor. Un puente que ademas
    guarda el puntero en almacenamiento estatico se sigue rechazando, y la llamada directa sigue aceptada.
Usage: foreign_import_bridge_escape.py <nsc.exe>
"""
from pathlib import Path
import subprocess
import sys
import tempfile

PROVIDER = '''namespace Probe.Raw
{
    public struct Status
    {
        public uint Length;
        public ulong Total;
    }

    public class Kernel
    {
        public static Status* last;

        public static int GlobalMemoryStatusEx(Status* p0)
        {
            unsafe
            {
                return __Native_0.GlobalMemoryStatusEx(p0);
            }
        }

        public static int RememberAndQuery(Status* p0)
        {
            last = p0;
            unsafe
            {
                return __Native_0.GlobalMemoryStatusEx(p0);
            }
        }
    }

    class __Native_0
    {
        extern C from "kernel32.dll" {
            public int GlobalMemoryStatusEx(Status* lpBuffer);
        }
    }
}
'''

WRAPPER = '''    public static class Facade
    {
        public static int Query(Status* buffer) { return Kernel.GlobalMemoryStatusEx(buffer); }
    }
'''

CASES = {
    'bridge-direct': (True, 'Kernel.GlobalMemoryStatusEx(&status)'),
    'bridge-through-wrapper': (True, 'Facade.Query(&status)'),
    'bridge-that-retains': (False, 'Kernel.RememberAndQuery(&status)'),
}

DIRECT = '''namespace Probe
{
    public struct Status { public uint Length; public ulong Total; }
    class Native
    {
        extern C from "kernel32.dll" { public int GlobalMemoryStatusEx(Status* lpBuffer); }
    }
    public static class Memory
    {
        public static bool Read()
        {
            Status status;
            status.Length = 64u;
            return Native.GlobalMemoryStatusEx(&status) != 0;
        }
    }
}
'''


def run(command, cwd):
    return subprocess.run([str(part) for part in command], cwd=cwd,
                          capture_output=True, text=True, errors='replace')


def project(name, output, extra=''):
    return ('<Project><PropertyGroup><Name>' + name + '</Name><OutputType>' + output +
            '</OutputType><Language>3</Language></PropertyGroup><ItemGroup><Compile Include="' +
            name + '.ns" />' + extra + '</ItemGroup></Project>')


def consumer_source(call):
    return ('using Probe.Raw;\nnamespace Probe\n{\n' + WRAPPER +
            '    public static class Memory\n    {\n        public static bool Read()\n        {\n'
            '            Status status;\n            status.Length = 64u;\n            return ' + call +
            ' != 0;\n        }\n    }\n}\n')


def main():
    compiler = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix='nsc-foreign-bridge-') as temporary:
        root = Path(temporary)
        (root / 'Provider.ns').write_text(PROVIDER, encoding='utf-8')
        (root / 'Provider.nsproj').write_text(project('Provider', 'SharedLibrary'), encoding='utf-8')
        result = run([compiler, '--emit-project-oop-dll', root / 'Provider.nsproj', '-O0',
                      '-o', root / 'Provider.dll', '--no-artifact-cache'], root)
        assert result.returncode == 0, ('provider', result.stdout + result.stderr)
        reference = ('<LibraryReference Include="' + (root / 'Provider.lib').as_posix() + '" Metadata="' +
                     (root / 'Provider.nsmeta').as_posix() + '" />')
        for name, (accepted, call) in CASES.items():
            (root / 'Consumer.ns').write_text(consumer_source(call), encoding='utf-8')
            (root / 'Consumer.nsproj').write_text(project('Consumer', 'Library', reference), encoding='utf-8')
            result = run([compiler, '--check-project-method-bodies', root / 'Consumer.nsproj'], root)
            output = result.stdout + result.stderr
            if accepted:
                assert result.returncode == 0, (name, output)
                for level in ('O0', 'O1', 'O2', 'O3'):
                    built = run([compiler, '--emit-project-oop-lib', root / 'Consumer.nsproj', '-' + level,
                                 '-o', root / ('Consumer-' + name + '-' + level + '.lib'),
                                 '--no-artifact-cache'], root)
                    assert built.returncode == 0, (name, level, built.stdout + built.stderr)
                    print(name, level, 'built', flush=True)
                continue
            assert result.returncode != 0 and 'unknown escape contract' in output, (name, output)
            print(name, 'rejected', flush=True)
        (root / 'Direct.ns').write_text(DIRECT, encoding='utf-8')
        (root / 'Direct.nsproj').write_text(project('Direct', 'Library'), encoding='utf-8')
        result = run([compiler, '--check-project-method-bodies', root / 'Direct.nsproj'], root)
        assert result.returncode == 0, ('direct-import', result.stdout + result.stderr)
        print('direct-import accepted', flush=True)


if __name__ == '__main__':
    main()
