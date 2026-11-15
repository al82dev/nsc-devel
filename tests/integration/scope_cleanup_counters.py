"""NSC-NCSS-008: locals bound to `new` are destroyed once on every exit of their scope.

Example 11 (GoBoard) clones a board into a local with no visible delete and leaves the loop body
by `continue`. Since Freeze 3.0 (item 141) a local bound directly to `new` is its owner and is
released at scope exit. This program builds boards in loops left by `continue`, `break` and an
early `return`, nested one level, and checks that constructions and destructions match after
each call, plus the exact counts and results. 42 is success, at O0 and O2.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

SOURCE = '''class Board
{
    public static int Built;
    public static int Destroyed;
    public int[] Cells;
    public Board(int size) { Cells = new int[size]; Built = Built + 1; }
    ~Board() { delete[] Cells; Destroyed = Destroyed + 1; }
    public bool Play(int cell, out int captures)
    {
        captures = cell % 3;
        return cell % 2 == 0;
    }
}

class P
{
    static int Search(int limit)
    {
        int best = 0;
        for (int cell = 0; cell < limit; cell++)
        {
            if (cell == 1) { continue; }
            Board clone = new Board(9);
            clone.Cells[0] = cell;
            int caps = 0;
            if (!clone.Play(cell, out caps)) { continue; }
            if (cell == 8) { return best + 100; }
            best = best + caps + clone.Cells[0];
        }
        return best;
    }

    static int Nested()
    {
        int found = 0;
        for (int sym = 0; sym < 8; sym++)
        {
            Board asymmetric = new Board(5);
            for (int k = 0; k < 3; k++)
            {
                Board inner = new Board(2);
                if (k == 1) { break; }
                found = found + inner.Cells.Length;
            }
            if (sym == 6) { break; }
        }
        return found;
    }

    public static int Main()
    {
        int a = Search(6);
        if (Board.Built != Board.Destroyed) return 1;
        int builtAfterFirst = Board.Built;
        int b = Search(10);
        if (Board.Built != Board.Destroyed) return 2;
        int c = Nested();
        if (Board.Built != Board.Destroyed) return 3;
        // Search(6): cells 0,2,3,4,5 build (5); Search(10): 0,2..8 build (8); Nested: 7 outer + 14 inner.
        if (builtAfterFirst != 5 || Board.Built != 5 + 8 + 7 + 14) return 4;
        if (a != 9 || b != 115) return 5;
        if (c != 14) return 6;
        return 42;
    }
}
'''

PROJECT = ('<Project><PropertyGroup><Name>ScopeCleanup</Name><OutputType>Exe</OutputType>'
           '</PropertyGroup><ItemGroup><Compile Include="Program.ns" /></ItemGroup></Project>')


def run(args, cwd):
    return subprocess.run(args, cwd=cwd, capture_output=True, text=True, errors='replace', timeout=120)


def main():
    compiler = str(Path(sys.argv[1]).resolve())
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-scope-cleanup-') as temporary:
        root = Path(temporary)
        (root / 'Program.ns').write_text(SOURCE, encoding='utf-8')
        (root / 'Case.nsproj').write_text(PROJECT, encoding='utf-8')
        for level in ('O0', 'O2'):
            executable = root / (level + '.exe')
            build = run([compiler, '-' + level, '--emit-project-oop-exe', str(root / 'Case.nsproj'),
                         '-o', str(executable), '--no-artifact-cache', '--no-error-log'], root)
            if build.returncode:
                failures.append((level, 'build', build.stdout + build.stderr))
                continue
            executed = run([str(executable)], root)
            print(level, 'exit', executed.returncode, flush=True)
            if executed.returncode != 42:
                failures.append((level, 'run', executed.returncode))
    if failures:
        print('FAIL', failures, flush=True)
        return 1
    print('scope_cleanup_counters: PASS', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
