import os
import subprocess
import sys
import tempfile
from pathlib import Path


def run(command, cwd):
    return subprocess.run([str(value) for value in command], cwd=cwd, capture_output=True,
                          text=True, errors='replace', timeout=120)


def main():
    compiler = Path(sys.argv[1])
    prefix = '''borrowed struct View { public int Data; public View() { Data = 42; } }
struct Value { public int Data; public Value() { Data = 42; } }
struct Owner { public Value data; public Owner() { data = new Value(); }
 ~Owner() { } public ref readonly Value Get() { return ref data; }
 public View Borrow() { return new View(); } }
'''
    cases = {
        'read': ('''class Program { public static int Work(Owner owner) {
 match (owner.Get()) { ref item => { return item.Data; } } return 1; }
 public static int Main() { return Work(new Owner()); } }''', None),
        'overwrite': ('''class Program { public static int Work(Owner owner) {
 match (owner.Get()) { ref item => { owner = new Owner(); return item.Data; } } return 1; }
 public static int Main() { return 0; } }''', 'cannot overwrite owner'),
        'move': ('''class Program { public static int Work(Owner owner) {
 match (owner.Get()) { ref item => { Owner other = move(owner); return item.Data; } } return 1; }
 public static int Main() { return 0; } }''', 'cannot move'),
        'write': ('''class Program { public static int Work(Owner owner) {
 match (owner.Get()) { ref item => { item.Data = 1; } } return 1; }
 public static int Main() { return 0; } }''', 'not assignable'),
        'finished': ('''class Program { public static int Work(Owner owner) {
 match (owner.Get()) { ref item => { if (item.Data != 42) return 1; } }
 owner = new Owner(); return owner.Get().Data; }
 public static int Main() { return Work(new Owner()); } }''', None),
        'ordinary_receiver': ('''class Box { public int Data;
 public ref[this] int Get() { return ref Data; } }
 class Program { public static int Main() { Box box = new Box(); box.Data = 42;
 match (box.Get()) { ref item => { return item; } } return 1; } }''', None),
        'mixed': ('''struct Pair { public int First; public int Second; }
 class Program { public static int Main() { Pair pair = default(Pair); pair.First = 40;
 match (pair) { { First: ref a, Second: ref mut b } => { b = a + 2; }, _ => { return 1; } }
 return pair.Second; } }''', None),
        'temporary': ('''class Program { public static Owner Make() { return new Owner(); }
 public static int Main() { match (Make().Get()) { ref item => { return item.Data; } }
 return 1; } }''',
                      'borrowed local requires'),
        'shadow_unrelated_overwrite': ('''class Program { public static int Main() {
 Owner owner = new Owner(); View view = owner.Borrow();
 { Owner owner = new Owner(); owner = new Owner(); } return view.Data; } }''', None),
        'shadow_unrelated_move': ('''class Program { public static int Main() {
 Owner owner = new Owner(); View view = owner.Borrow();
 { Owner owner = new Owner(); Owner moved = move(owner); } return view.Data; } }''', None),
        'shadow_inner_borrowed': ('''class Program { public static int Main() {
 Owner owner = new Owner(); { Owner owner = new Owner(); View view = owner.Borrow();
 owner = new Owner(); return view.Data; } } }''', 'cannot overwrite owner'),
        'shadow_outer_borrowed': ('''class Program { public static int Main() {
 Owner owner = new Owner(); View view = owner.Borrow(); { Owner owner = new Owner(); }
 owner = new Owner(); return view.Data; } }''', 'cannot overwrite owner'),
    }
    with tempfile.TemporaryDirectory(prefix='nsc-readonly-pattern-') as temporary:
        root = Path(temporary)
        project = root / 'Pattern.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Pattern</Name><Language>3</Language>'
                           '</PropertyGroup><ItemGroup><Compile Include="*.ns" /></ItemGroup></Project>',
                           encoding='utf-8')
        for name, (source, diagnostic) in cases.items():
            (root / 'Main.ns').write_text(prefix + source, encoding='utf-8')
            result = run([compiler, '--check-project-method-bodies', project], root)
            output = result.stdout + result.stderr
            if diagnostic is not None:
                assert result.returncode != 0 and diagnostic in output, (name, output)
                print(name, 'rejected:', diagnostic, flush=True)
                continue
            assert result.returncode == 0, (name, output)
            if os.name == 'nt':
                for level in ('O0', 'O2'):
                    executable = root / (name + '-' + level + '.exe')
                    result = run([compiler, '--emit-project-oop-exe', project, '-o', executable,
                                  '-' + level], root)
                    assert result.returncode == 0, (name, level, result.stdout + result.stderr)
                    result = run([executable], root)
                    assert result.returncode == 42, (name, level, result.returncode)
                    print(name, level, 'exit=42', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
