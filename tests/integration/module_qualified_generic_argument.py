"""Module-qualified type argument in an expression (`Token<Module::Ns.Type>.Get()`)
(docs/fixes/argumento-generico-calificado-por-modulo.md).

EN: The metadata producer writes a public template body with module-qualified type names, so
    `Token<Color>.Get()` becomes `Token<Case::Lib.Graphics.Color>.Get()` in the `.nsmeta` template
    surface. The parser's generic-argument lookahead rejected `::` and read the expression as
    comparisons, so the producer's read-back verification failed and the library was not emitted.
    The library now emits at O0..O3, the qualified form is also accepted in source, and ternaries
    whose branches look like generic arguments still parse as ternaries.
ES: El productor de metadatos escribe el cuerpo de una plantilla publica con nombres de tipo
    calificados por modulo, asi `Token<Color>.Get()` queda `Token<Case::Lib.Graphics.Color>.Get()` en
    la superficie de plantillas del `.nsmeta`. La vista anticipada de argumentos genericos del parser
    rechazaba `::` y leia la expresion como comparaciones; la verificacion de relectura del productor
    fallaba y la biblioteca no se emitia. Ahora la biblioteca se emite en O0..O3, la forma calificada
    tambien se acepta en el fuente y los ternarios cuyas ramas parecen argumentos genericos siguen
    leyendose como ternarios.
Usage: module_qualified_generic_argument.py <nsc.exe>
"""
from pathlib import Path
import subprocess
import sys
import tempfile

LIBRARY = '''namespace Lib.Graphics
{
    public struct Color
    {
        public int V;
    }

    public class Token<T>
    {
        private static int seed;
        public static int Get() { seed = seed + 1; return seed; }
    }

    public class Texture
    {
        public bool IsColor<T>(T[] data)
        {
            if (Token<T>.Get() == Token<Color>.Get()) { return true; }
            return false;
        }

        public bool IsQualified<T>(T[] data)
        {
            return Token<T>.Get() == Token<Case::Lib.Graphics.Color>.Get();
        }

        public int Pick(int a, int b, bool flag)
        {
            int low = a < b ? a : b;
            int high = flag ? a : b > a ? b : a;
            return a < b == flag ? low : high;
        }
    }
}
'''


def run(command, cwd):
    return subprocess.run([str(part) for part in command], cwd=cwd,
                          capture_output=True, text=True, errors='replace')


def main():
    compiler = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix='nsc-qualified-generic-') as temporary:
        root = Path(temporary)
        (root / 'Program.ns').write_text(LIBRARY, encoding='utf-8')
        case = root / 'Case.nsproj'
        case.write_text('<Project><PropertyGroup><Name>Case</Name><OutputType>Library</OutputType>'
                        '<Language>3</Language></PropertyGroup><ItemGroup><Compile Include="Program.ns" />'
                        '</ItemGroup></Project>', encoding='utf-8')
        for level in ('O0', 'O1', 'O2', 'O3'):
            output = root / ('case-' + level + '.lib')
            result = run([compiler, '--emit-project-oop-lib', case, '-' + level, '-o', output,
                          '--no-artifact-cache'], root)
            assert result.returncode == 0, (level, result.stdout + result.stderr)
            metadata = output.with_suffix('.nsmeta')
            assert metadata.exists(), (level, 'missing metadata')
            print('module-qualified-generic-argument', level, 'emitted', flush=True)


if __name__ == '__main__':
    main()
