"""NSC-NCSS-006: an `out` parameter is assigned on every path that leaves the method normally.

Until 2026-11-14 the compiler only checked that an `out` parameter was written somewhere in the
body, so `if (depth == 0) { return 3; }` before the only assignment compiled and handed the caller
whatever the slot held (the AlphaBeta shape of example 03). The check now runs at each reachable
`return` and at the reachable end of the body, with the path state the ownership flow joins at
branches, loops and handlers.

The valid programs cover the shapes that must stay accepted -- both branches, an early return that
assigns, a loop after an assignment, `while (true)` with `break`, `throw` and `panic` paths, a
try/catch where both assign, forwarding through another `out`, a lambda in the body, field-wise
struct assignment and a recursive alpha-beta search -- and check their values at O0 and O2. The
invalid ones each leave one path unassigned and must be rejected with the exact message.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

CORE = Path(__file__).resolve().parents[2] / 'lib' / 'NativeSharp.Core' / 'NativeSharp.Core.nsproj'

PATHS = '''using NativeSharp.Core;

class Paths
{
    public static void Both(bool c, out int x) { if (c) { x = 1; } else { x = 2; } }
    public static int Early(int d, out int x) { if (d == 0) { x = 0; return 0; } x = d; return d; }
    public static void Loop(out int x) { x = 0; for (int i = 0; i < 3; i++) { x = x + i; } }
    public static void Forever(out int x)
    {
        int i = 0;
        while (true) { i = i + 1; if (i == 4) { x = i; break; } }
    }
    public static void Throws(bool c, out int x) throws Exception { if (c) { throw new Exception(); } x = 5; }
    public static void Panics(bool c, out int x) { if (c) { x = 6; return; } panic("no value"); }
    public static void Caught(out int x) { try { x = 1; } catch (Exception e) { x = 2; } }
    public static void Forwarded(out int x) { Both(true, out x); }
    public static void Lambda(out int x) { x = 3; int captured = 4; Reader r = () => captured; x = x + r(); }
    public static void Fields(out Pair p) { p.Left = 1; p.Right = 2; }
}

delegate int Reader();

struct Pair { public int Left; public int Right; }

int Main()
{
    int a; int b; int c; int d; int f; int g; int h; int k; Pair p;
    Paths.Both(false, out a);
    Paths.Early(3, out b);
    Paths.Loop(out c);
    Paths.Forever(out d);
    Paths.Panics(true, out f);
    Paths.Caught(out g);
    Paths.Forwarded(out h);
    Paths.Lambda(out k);
    Paths.Fields(out p);
    int e = 0;
    try { Paths.Throws(false, out e); } catch (Exception error) { return 90; }
    return a + b + c + d + e + f + g + h + k + p.Left + p.Right;
}
'''

ALPHA_BETA = '''public struct Move
{
    public int From;
    public int To;
    public Move(int from, int to) { From = from; To = to; }
}

public struct SearchResult
{
    public int Score;
    public Move Best;
}

class Search
{
    // Alpha-beta shaped: the best move comes back through `out`, the score as the return value.
    public static int AlphaBeta(int depth, int alpha, int beta, bool maximizing, out Move best)
    {
        if (depth == 0)
        {
            best = new Move(depth + 7, 12);
            return depth + 3;
        }
        best = new Move(0, 0);
        int value = maximizing ? -1000 : 1000;
        for (int child = 0; child < 3; child++)
        {
            Move candidate;
            int score = AlphaBeta(depth - 1, alpha, beta, !maximizing, out candidate) + child;
            if (maximizing ? score > value : score < value)
            {
                value = score;
                best = new Move(candidate.From + child, candidate.To);
            }
            if (maximizing && value > alpha) alpha = value;
            if (!maximizing && value < beta) beta = value;
            if (alpha >= beta) break;
        }
        return value;
    }

    // The same search returning both through a value struct.
    public static SearchResult ByValue(int depth, bool maximizing)
    {
        SearchResult result;
        if (depth == 0)
        {
            result.Score = 3;
            result.Best = new Move(7, 12);
            return result;
        }
        result.Score = maximizing ? -1000 : 1000;
        result.Best = new Move(0, 0);
        for (int child = 0; child < 3; child++)
        {
            SearchResult inner = ByValue(depth - 1, !maximizing);
            int score = inner.Score + child;
            if (maximizing ? score > result.Score : score < result.Score)
            {
                result.Score = score;
                result.Best = new Move(inner.Best.From + child, inner.Best.To);
            }
        }
        return result;
    }
}

int Main()
{
    Move best;
    int score = Search.AlphaBeta(3, -1000, 1000, true, out best);
    SearchResult plain = Search.ByValue(3, true);
    if (best.To != 12) return 1;
    if (Search.AlphaBeta(0, -1000, 1000, true, out best) != 3 || best.From != 7) return 2;
    return score * 10 + best.From;
}
'''

VALID = {'paths': (PATHS, 35), 'alpha-beta': (ALPHA_BETA, 77)}

INVALID = {
    'early return': ('''class S { public static int F(int d, out int x) { if (d == 0) { return 3; } x = d; return 1; } }
int Main() { int x; return S.F(1, out x); }
''',
        ":1:65: error: out parameter 'x' is not assigned on every path that leaves 'F'"),
    'if without else': ('''class S { public static void F(bool c, out int x) { if (c) { x = 1; } } }
int Main() { int x; S.F(true, out x); return 0; }
''',
        "out parameter 'x' is not assigned on every path that leaves 'F'"),
    'assignment only inside a loop': ('''class S {
 public static void F(int n, out int x) { for (int i = 0; i < n; i++) { x = i; } } }
int Main() { int x; S.F(2, out x); return 0; }
''',
        "out parameter 'x' is not assigned on every path that leaves 'F'"),
    'catch path without assignment': ('''using NativeSharp.Core;
class S { public static void F(out int x) { try { x = 1; } catch (Exception e) { } } }
int Main() { int x; S.F(out x); return 0; }
''',
        "out parameter 'x' is not assigned on every path that leaves 'F'"),
    'struct fields on one path': ('''struct Move { public int From; public int To; }
class S { public static int F(int depth, out Move best) {
 if (depth == 0) { return 3; } best.From = depth; best.To = 12; return F(depth - 1, out best); } }
int Main() { Move best; return S.F(2, out best); }
''',
        "out parameter 'best' is not assigned on every path that leaves 'F'"),
    'recursive search, base case unassigned': ('''public struct Move
{
    public int From;
    public int To;
}

class Search
{
    public static int AlphaBeta(int depth, out Move best)
    {
        if (depth == 0) { return 3; }
        best.From = depth;
        best.To = 12;
        return AlphaBeta(depth - 1, out best);
    }
}

int Main()
{
    Move best;
    return Search.AlphaBeta(2, out best);
}
''',
        "out parameter 'best' is not assigned on every path that leaves 'AlphaBeta'"),
}


def project():
    return ('<Project><PropertyGroup><Name>OutPaths</Name><OutputType>Exe</OutputType></PropertyGroup>'
            '<ItemGroup><Compile Include="Program.ns" /><ProjectReference Include="' + str(CORE) +
            '" /></ItemGroup></Project>')


def run(args, cwd):
    return subprocess.run(args, cwd=cwd, capture_output=True, text=True, errors='replace', timeout=300)


def main():
    compiler = str(Path(sys.argv[1]).resolve())
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-out-paths-') as temporary:
        root = Path(temporary)

        def write(folder, source):
            folder.mkdir()
            (folder / 'Program.ns').write_text(source, encoding='utf-8')
            (folder / 'Case.nsproj').write_text(project(), encoding='utf-8')
            return folder / 'Case.nsproj'

        for name, (source, expected) in VALID.items():
            for level in ('O0', 'O2'):
                folder = root / (name + '-' + level)
                case = write(folder, source)
                build = run([compiler, '-' + level, '--emit-project-oop-exe', str(case), '-o',
                             str(folder / 'case.exe'), '--no-artifact-cache', '--no-error-log'], folder)
                if build.returncode:
                    failures.append((name, level, 'build', build.stdout + build.stderr))
                    continue
                executed = run([str(folder / 'case.exe')], folder)
                print(name, level, 'exit', executed.returncode, flush=True)
                if executed.returncode != expected:
                    failures.append((name, level, 'run', executed.returncode))
        for index, (name, (source, expected)) in enumerate(INVALID.items()):
            case = write(root / ('invalid-' + str(index)), source)
            result = run([compiler, '--check-project-method-bodies', str(case)], case.parent)
            output = result.stdout + result.stderr
            print(name, 'exit', result.returncode, flush=True)
            if result.returncode == 0 or expected not in output:
                failures.append((name, result.returncode, output))
    if failures:
        print('FAIL', failures, flush=True)
        return 1
    print('out_parameter_paths: PASS', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
