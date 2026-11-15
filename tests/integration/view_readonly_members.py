"""Read-only members of StringView, Span<T> and ReadOnlySpan<T> while derived views are alive.

Since the 2026-11 view rules, a method that is not `readonly` counts as potentially mutating, and
it cannot be called on a value while a view obtained from that value is alive. The SDK views
declared only Length/IsEmpty/the read-only indexer as `readonly`, so Slice, Data, IndexOf,
StartsWith, CopyTo and the rest were rejected in the ordinary pattern "take a slice, keep using
the original". The valid program uses each of them in that pattern and checks the values.

The two invalid programs keep the rule honest: a user view whose method is not `readonly` is still
rejected in the same pattern, and a `readonly` method that assigns a field does not compile, so
marking the SDK methods `readonly` is a property the compiler verified, not a label.
"""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

CORE = Path(__file__).resolve().parents[2] / 'lib' / 'NativeSharp.Core' / 'NativeSharp.Core.nsproj'

VALID = '''using NativeSharp.Core;

class P
{
    static int Text()
    {
        StringView trimmed = StringView.Of("  hola mundo  ").Trim();
        StringView first = trimmed.Slice(0, 4);
        StringView second = trimmed.Slice(5);
        if (!StringView.Equals(first, "hola") || !StringView.Equals(second, "mundo")) return 1;
        if (trimmed.IndexOf(' ') != 4 || trimmed.LastIndexOf('o') != 9) return 2;
        if (!trimmed.Contains('m') || !trimmed.StartsWith(first) || !trimmed.EndsWith(second)) return 3;
        if (trimmed.StartsWith(second) || trimmed.UncheckedAt(0) != 'h') return 4;
        if (trimmed.Data() + 5 != second.Data()) return 5;
        if (trimmed.TrimStart().Length() != 10 || trimmed.TrimEnd().Length() != 10) return 6;
        raii string copy = first.ToString();
        if (copy.Length != 4) return 7;
        return 0;
    }

    static int Numbers()
    {
        raii int[] numbers = new int[6];
        raii int[] other = new int[6];
        Span<int> destination = Spans.Of<int>(other);
        Span<int> all = Spans.Of<int>(numbers);
        all.Fill(3);
        all[5] = 20;
        Span<int> tail = all.Slice(2);
        if (tail.Data() != all.Data() + 2 || tail.IsEmpty() || all.IsEmpty()) return 10;
        if (all.Slice(1, 2).Length() != 2 || all.UncheckedAt(5) != 20) return 11;
        if (all.CopyTo(destination) != 6 || other[5] != 20) return 12;
        Span<int> front = all.Slice(0, 3);
        if (all.CopyToOverlapping(tail) != 4 || numbers[2] != 3) return 13;
        ReadOnlySpan<int> view = all.AsReadOnly();
        ReadOnlySpan<int> rest = view.Slice(1);
        if (view.Data() + 1 != rest.Data() || view.IsEmpty() || view.UncheckedAt(0) != 3) return 14;
        if (view.Slice(0, 2).Length() != 2 || front.Length() != 3) return 15;
        return 0;
    }

    public static int Main()
    {
        return Text() + Numbers();
    }
}
'''

MUTATING = '''using NativeSharp.Core;

public copy borrowed struct Window
{
    private char* start;
    private int count;

    public Window(char* data, int length)
    {
        start = data;
        count = length;
    }

    public readonly Window Part(int length) { return new Window(start, length); }

    public Window Shift() { return new Window(start + 1, count - 1); }
}

class P
{
    public static int Main()
    {
        StringView text = StringView.Of("abcdef");
        Window whole = new Window(text.Data(), 6);
        Window part = whole.Part(2);
        Window shifted = whole.Shift();
        return 0;
    }
}
'''

READONLY_ASSIGNS = '''public copy borrowed struct Counter
{
    private int count;

    public Counter(int value) { count = value; }

    public readonly int Reset()
    {
        count = 0;
        return count;
    }
}

class P
{
    public static int Main() { return 0; }
}
'''

INVALID = {
    'non-readonly method with a live view': (
        MUTATING, 'cannot call potentially mutating method while a borrowed field view is alive'),
    'readonly method assigning a field': (READONLY_ASSIGNS, 'left side of assignment is not assignable'),
}


def project(name):
    return ('<Project><PropertyGroup><Name>' + name + '</Name><OutputType>Exe</OutputType></PropertyGroup>'
            '<ItemGroup><Compile Include="Program.ns" /><ProjectReference Include="' + str(CORE) +
            '" /></ItemGroup></Project>')


def run(args, cwd):
    return subprocess.run(args, cwd=cwd, capture_output=True, text=True, errors='replace', timeout=300)


def main():
    compiler = str(Path(sys.argv[1]).resolve())
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-view-readonly-') as temporary:
        root = Path(temporary)

        def build(folder, source, level):
            folder.mkdir()
            (folder / 'Program.ns').write_text(source, encoding='utf-8')
            (folder / 'Case.nsproj').write_text(project('ViewReadonly'), encoding='utf-8')
            return run([compiler, '-' + level, '--emit-project-oop-exe', str(folder / 'Case.nsproj'),
                        '-o', str(folder / 'case.exe'), '--no-artifact-cache', '--no-error-log'], folder)

        for level in ('O0', 'O2'):
            folder = root / ('valid-' + level)
            result = build(folder, VALID, level)
            if result.returncode:
                failures.append((level, 'valid build', result.stdout + result.stderr))
                continue
            executed = run([str(folder / 'case.exe')], folder)
            print(level, 'valid exit', executed.returncode, flush=True)
            if executed.returncode != 0:
                failures.append((level, 'valid run', executed.returncode))
        for index, (name, (source, expected)) in enumerate(INVALID.items()):
            result = build(root / ('invalid-' + str(index)), source, 'O0')
            output = result.stdout + result.stderr
            print(name, 'exit', result.returncode, flush=True)
            if result.returncode == 0 or expected not in output:
                failures.append((name, result.returncode, output))
    if failures:
        print('FAIL', failures, flush=True)
        return 1
    print('view_readonly_members: PASS', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
