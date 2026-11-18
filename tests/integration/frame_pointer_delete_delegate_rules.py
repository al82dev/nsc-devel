"""Ownership and frame-storage rules for delegates, raw pointers and delete (LKF-003/005/017/020/021).

Each case either runs and exits 42 at O0 and O2, or is rejected with the exact diagnostic. Every
rejection has an accepted control that differs only in the property the rule checks:

- moving a raii delegate clears the source, so the cell is released once (it used to be released
  twice and corrupt the heap);
- a method group binds to a delegate only when every parameter has the same ref/out/in keyword;
- a raii delegate or dynamic array is not copied into a plain local, which would alias it;
- a pointer to a local, a by-value parameter, a fixed array element or stackalloc storage is not
  returned, while a pointer to heap, static or caller storage is;
- delete rejects borrowed operands, frame addresses and scalar delete of a dynamic array.
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

FRAME = "a pointer to current-frame storage cannot be returned; its stack frame ends with this call"
DELETE_FRAME = ("a pointer to current-frame storage (a local or stackalloc) was not allocated with new "
                "and cannot be deleted")
BORROWED = "is owned by the caller and cannot be deleted"
COPY = "cannot be copied into a non-owning local"

CASES = {
    'raii delegate move, static target': ('''delegate int Transform(int value);
class Program { static int Read(int value) { return value; }
 public static int Main() { raii Transform source = Read; raii Transform target = move(source);
 return target(42); } }''', 'run'),
    'raii delegate move, bound receiver': ('''delegate int Transform(int value);
class Worker { public int Bias; public int Apply(int value) { return value + Bias; } }
class Program { public static int Main() { Worker w = new Worker(); w.Bias = 1; raii Transform source = w.Apply;
 raii Transform target = move(source); return target(41); } }''', 'run'),
    'raii delegate move chain, capturing lambda': ('''delegate int Transform(int value);
class Program { public static int Main() { int bias = 2; raii Transform source = x => x + bias;
 raii Transform target = move(source); raii Transform third = move(target); return third(40); } }''', 'run'),
    'delegate ref parameter bound to in method': ('''delegate void Mutator(ref int value);
class Program { static void Read(in int value) { }
 public static int Main() { Mutator item = Read; return 42; } }''',
        "no method in the method group exactly matches delegate signature"),
    'delegate ref parameter bound to ref method': ('''delegate void Mutator(ref int value);
class Program { static void Bump(ref int value) { value = value + 1; }
 public static int Main() { Mutator item = Bump; int v = 41; item(ref v); return v; } }''', 'run'),
    'raii delegate copied to plain local': ('''delegate int Transform(int value);
class Program { static int Read(int value) { return value; }
 public static int Main() { raii Transform owner = Read; Transform copy = owner; return 42; } }''', COPY),
    'plain delegate copied to plain local': ('''delegate int Transform(int value);
class Program { static int Read(int value) { return value; }
 public static int Main() { Transform first = Read; Transform copy = first; return copy(42); } }''', 'run'),
    'raii array copied to plain local': ('''class Program { public static int Main() {
 raii int[] source = new int[2]; int[] copy = source; return 42; } }''', COPY),
    'raii array borrowed by a borrowed local': ('''class Program { public static int Main() {
 raii int[] source = new int[2]; source[1] = 42; borrowed int[] view = source; return view[1]; } }''', 'run'),
    'plain array copied to plain local': ('''class Program { public static int Main() {
 int[] source = new int[2]; source[1] = 42; int[] copy = source; return copy[1]; } }''', 'run'),
    'return address of local': ('''class Probe { public static int* Leak() { int local = 42; return &local; }
 public static int Main() { return 42; } }''', FRAME),
    'return address of by-value parameter': ('''class Probe { public static int* Leak(int value) { return &value; }
 public static int Main() { return 42; } }''', FRAME),
    'return address of fixed array element': ('''class Probe {
 public static int* Leak() { int[4] fixed; fixed[0] = 1; return &fixed[0]; }
 public static int Main() { return 42; } }''', FRAME),
    'return stackalloc through a local': ('''class Probe {
 public static int* Leak() { int* storage = stackalloc int[1]; return storage; }
 public static int Main() { return 42; } }''', FRAME),
    'return stackalloc directly': ('''class Probe { public static int* Leak() { return stackalloc int[1]; }
 public static int Main() { return 42; } }''', FRAME),
    'return offset into stackalloc': ('''class Probe {
 public static int* Leak() { int* s = stackalloc int[4]; return s + 1; }
 public static int Main() { return 42; } }''', FRAME),
    'return heap pointer': ('''class Probe {
 public static int* Make() { int* value = new int; *value = 42; return value; }
 public static int Main() { int* p = Make(); int r = *p; delete p; return r; } }''', 'run'),
    'return caller pointer': ('''class Probe { public static int* Pass(int* value) { return value; }
 public static int Main() { int x = 42; int* p = Pass(&x); return *p; } }''', 'run'),
    'return dynamic array element address': ('''class Probe { static int[] Kept = new int[4];
 public static int* At(int[] values) { return &values[1]; }
 public static int* AtLocal() { int[] values = Kept; return &values[2]; }
 public static int Main() { Kept[1] = 40; Kept[2] = 2; return *At(Kept) + *AtLocal(); } }''', 'run'),
    'return pointer rebuilt from a heap address': ('''class Probe {
 static long[] Block = new long[4];
 public static byte* At() { long* address = &Block[0]; long start = (long)address + 8l; return (byte*)start; }
 public static int Main() { Block[1] = 42l; return (int)*((long*)At()); } }''', 'run'),
    'return field of a ref struct parameter': ('''struct Cell { public int Value; }
class Probe { public static int* At(ref Cell cell) { return &cell.Value; }
 public static int Main() { Cell c; c.Value = 42; return *At(ref c); } }''', 'run'),
    'delete stackalloc pointer': ('''class Program { public static int Main() {
 int* value = stackalloc int[1]; delete value; return 42; } }''', DELETE_FRAME),
    'delete address of local': ('''class Program { public static int Main() {
 int x = 1; int* value = &x; delete value; return 42; } }''', DELETE_FRAME),
    'delete heap pointer': ('''class Program { public static int Main() {
 int* v = new int; *v = 42; int r = *v; delete v; return r; } }''', 'run'),
    'delete borrowed parameter': ('''class Item { } class Program {
 private static void Release(borrowed Item value) { delete value; }
 public static int Main() { return 42; } }''', BORROWED),
    'delete in parameter': ('''class Item { } class Program {
 private static void Release(in Item value) { delete value; } public static int Main() { return 42; } }''', BORROWED),
    'delete owned parameter': ('''class Item { } class Program {
 private static void Release(owned Item value) { delete value; }
 public static int Main() { Release(new Item()); return 42; } }''', 'run'),
    'scalar delete of dynamic array': ('''class Program { public static int Main() {
 int[] values = new int[2]; delete values; return 42; } }''',
        "a dynamic array is released with delete[], not with scalar delete"),
    'array delete of dynamic array': ('''class Program { public static int Main() {
 int[] values = new int[2]; delete[] values; return 42; } }''', 'run'),
}

PROJECT = ('<Project><PropertyGroup><Name>Rules</Name><OutputType>Exe</OutputType>'
           '</PropertyGroup><ItemGroup><Compile Include="Program.ns" /></ItemGroup></Project>')


def run(args, cwd):
    return subprocess.run(args, cwd=cwd, capture_output=True, text=True, errors='replace', timeout=120)


def main():
    compiler = str(Path(sys.argv[1]).resolve())
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-frame-pointer-rules-') as temporary:
        root = Path(temporary)
        for index, (name, (source, expected)) in enumerate(CASES.items()):
            folder = root / ('case-' + str(index))
            folder.mkdir()
            (folder / 'Program.ns').write_text(source, encoding='utf-8')
            project = folder / 'Case.nsproj'
            project.write_text(PROJECT, encoding='utf-8')
            if expected != 'run':
                result = run([compiler, '--check-project-method-bodies', str(project)], folder)
                output = result.stdout + result.stderr
                print(name, 'exit', result.returncode, flush=True)
                if result.returncode == 0 or expected not in output:
                    failures.append((name, 'rejection', output.strip()[-300:]))
                continue
            for level in ('O0', 'O2'):
                exe = folder / ('case-' + level + '.exe')
                build = run([compiler, '-' + level, '--emit-project-oop-exe', str(project), '-o', str(exe),
                             '--no-artifact-cache', '--no-error-log'], folder)
                if build.returncode:
                    failures.append((name, level, 'build', (build.stdout + build.stderr).strip()[-300:]))
                    continue
                executed = run([str(exe)], folder)
                print(name, level, 'exit', executed.returncode, flush=True)
                if executed.returncode != 42:
                    failures.append((name, level, 'run', executed.returncode))
    if failures:
        print('FAIL', failures, flush=True)
        return 1
    print('frame_pointer_delete_delegate_rules: PASS', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
