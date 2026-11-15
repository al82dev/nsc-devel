"""Values and members rebuilt from library metadata: LKF-260, LKF-261, LKF-272 and LKF-347.

A consumer of a precompiled library sees the library through source rebuilt from its .nsmeta. Each case
builds a small library, then a consumer that uses the rebuilt declaration, at O0 and O2:

- LKF-260: a char default outside printable ASCII (a control byte, 0x80, 0xFF) and a string constant
  with control and non-ASCII bytes are spelled with escapes the lexer decodes to the same bytes.
- LKF-261: infinite and NaN constants are spelled as the divisions that fold to them; finite values
  keep their exact bits.
- LKF-347: a precompiled class with field initializers runs them when the consumer constructs it,
  through the implicit and through an explicit constructor.
- LKF-272: a set-only property never reaches metadata: the library build rejects it, so the
  materializer's getter requirement matches the language.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

LIBRARY_PROJECT = ('<Project><PropertyGroup><Name>Values</Name><OutputType>Library</OutputType></PropertyGroup>'
                   '<ItemGroup><Compile Include="*.ns" /></ItemGroup></Project>')

CHAR_LIBRARY = ('namespace Values { public class Chars { '
                'public static int Bell(char c = \'\\x07\') { return c == \'\\x07\' ? 1 : 100; } '
                'public static int High(char c = \'\\x80\') { return c == \'\\x80\' ? 1 : 100; } '
                'public static int Top(char c = \'\\xFF\') { return c == \'\\xFF\' ? 1 : 100; } '
                'public static int Quote(char c = \'\\\'\') { return c == \'\\\'\' ? 1 : 100; } '
                'public static int Zero(char c = \'\\0\') { return c == \'\\0\' ? 1 : 100; } } }\n')
FLOAT_LIBRARY = ('namespace Values { public class Floats { '
                 'public const double Infinite = 1.0 / 0.0; '
                 'public const double NegativeInfinite = -1.0 / 0.0; '
                 'public const double NotANumber = 0.0 / 0.0; '
                 'public const float SingleInfinite = 1.0f / 0.0f; '
                 'public const double Third = 1.0 / 3.0; '
                 'public const float SingleThird = 1.0f / 3.0f; '
                 'public static int Same(double value) { return value == 1.0 / 3.0 ? 1 : 100; } '
                 'public static int SameSingle(float value) { return value == 1.0f / 3.0f ? 1 : 100; } } }\n')
STRING_LIBRARY = ('namespace Values { public class Texts { '
                  'public const string Controls = "a\\x01b\\x1Fc\\x7F"; '
                  'public const string Accented = "\\u00E9t\\u00E9"; } }\n')

INIT_LIBRARY = ('namespace Values { public class Settings { public int Width = 40; public int Height = 2; '
                'public int Area() { return Width + Height; } } '
                'public class Explicit { public int Width = 40; public Explicit() { Width = Width + 2; } } }\n')
SINK_LIBRARY = ('namespace Values { public class Sink { public static int Seen = 0; '
                'public int Value { set { Seen = value; } } } }\n')

CASES = {
    'char defaults outside printable ASCII': (CHAR_LIBRARY,
        'using Values;\nclass Program { public static int Main() { '
        'return 37 + Chars.Bell() + Chars.High() + Chars.Top() + Chars.Quote() + Chars.Zero(); } }', 42),
    'non-finite constants': (FLOAT_LIBRARY,
        'using Values;\nclass Program { public static int Main() { int r = 38; '
        'if (Floats.Infinite > 1e308) { r = r + 1; } if (Floats.NegativeInfinite < -1e308) { r = r + 1; } '
        'if (Floats.NotANumber != Floats.NotANumber) { r = r + 1; } '
        'if (Floats.SingleInfinite > 3e38f) { r = r + 1; } return r; } }', 42),
    'finite constants keep their bits': (FLOAT_LIBRARY,
        'using Values;\nclass Program { public static int Main() { '
        'return 40 + Floats.Same(Floats.Third) + Floats.SameSingle(Floats.SingleThird); } }', 42),
    'string constant with control and non-ASCII bytes': (STRING_LIBRARY,
        'using Values;\nclass Program { public static int Main() { string s = Texts.Controls; '
        'string t = Texts.Accented; '
        'if (s.Length != 6 || s[1] != \'\\x01\' || s[3] != \'\\x1F\' || s[5] != \'\\x7F\') { return 1; } '
        'if (t.Length != 5 || t[2] != \'t\') { return 2; } return 42; } }', 42),
    'field initializers of a precompiled class with an implicit constructor': (INIT_LIBRARY,
        'using Values;\nclass Program { public static int Main() { raii Settings s = new Settings(); '
        'return s.Area(); } }', 42),
    'field initializers of a precompiled class with an explicit constructor': (INIT_LIBRARY,
        'using Values;\nclass Program { public static int Main() { raii Explicit e = new Explicit(); '
        'return e.Width; } }', 42),
    'set-only property is rejected by the library build': (SINK_LIBRARY, None,
        'a property without one cannot be bound'),
}


def run(args, cwd):
    return subprocess.run(args, cwd=cwd, capture_output=True, text=True, errors='replace', timeout=600)


def main():
    compiler = str(Path(sys.argv[1]).resolve())
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-metadata-surface-') as temporary:
        root = Path(temporary)
        for index, (name, (library_source, consumer_source, expected)) in enumerate(CASES.items()):
            library = root / ('library-' + str(index))
            library.mkdir()
            (library / 'Values.ns').write_text(library_source, encoding='utf-8')
            (library / 'Values.nsproj').write_text(LIBRARY_PROJECT, encoding='utf-8')
            built = run([compiler, '--no-artifact-cache', '--emit-project-oop-lib', str(library / 'Values.nsproj'),
                         '-o', str(library / 'Values.lib')], library)
            if consumer_source is None:
                output = built.stdout + built.stderr
                print(name, 'library exit', built.returncode, flush=True)
                if built.returncode == 0 or expected not in output:
                    failures.append((name, 'library rejection', output.strip()[-400:]))
                continue
            if built.returncode:
                failures.append((name, 'library', (built.stdout + built.stderr).strip()[-400:]))
                continue
            consumer = root / ('consumer-' + str(index))
            consumer.mkdir()
            (consumer / 'Program.ns').write_text(consumer_source, encoding='utf-8')
            (consumer / 'Case.nsproj').write_text(
                '<Project><PropertyGroup><Name>Consumer</Name><OutputType>Exe</OutputType></PropertyGroup>'
                '<ItemGroup><Compile Include="*.ns" /></ItemGroup><ItemGroup><LibraryReference Include="{0}" '
                'Metadata="{1}" /></ItemGroup></Project>'.format((library / 'Values.lib').as_posix(),
                                                               (library / 'Values.nsmeta').as_posix()),
                encoding='utf-8')
            for level in ('O0', 'O2'):
                exe = consumer / ('case-' + level + '.exe')
                build = run([compiler, '-' + level, '--emit-project-oop-exe', str(consumer / 'Case.nsproj'), '-o',
                             str(exe), '--no-artifact-cache', '--no-error-log'], consumer)
                if build.returncode:
                    failures.append((name, level, 'build', (build.stdout + build.stderr).strip()[-400:]))
                    continue
                executed = run([str(exe)], consumer)
                print(name, level, 'exit', executed.returncode, flush=True)
                if executed.returncode != expected:
                    failures.append((name, level, 'exit', executed.returncode))
    if failures:
        for failure in failures:
            print('FAIL', failure)
        return 1
    print('PASS', len(CASES), 'cases')
    return 0


if __name__ == '__main__':
    sys.exit(main())
