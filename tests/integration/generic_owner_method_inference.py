"""Distinguish generic method owners for explicit and inferred calls."""
from pathlib import Path
import subprocess
import sys
import tempfile


def run(command, root):
    result = subprocess.run([str(value) for value in command], cwd=root, capture_output=True,
                            text=True, errors='replace', timeout=120)
    print('COMMAND', subprocess.list2cmdline([str(value) for value in command]),
          'EXIT', result.returncode, result.stdout + result.stderr, flush=True)
    return result


def main():
    compiler = Path(sys.argv[1]).resolve()
    prefix = '''class Trace { public static int Which; }
class Payload { }
namespace Alpha { public struct Box<T> {
 public U Echo<U>(U value) { Trace.Which = 1; return value; }
 public U Echo<U>(U[] values) { Trace.Which = 3; return values[0]; }
 public static U Static<U>(U value) { return value; }
 public int ClassOnly<U>(U value) where U : class { return 42; } } }
namespace Beta { public struct Box<T> {
 public U Echo<U>(U value) { Trace.Which = 2; return value; } } }
'''
    cases = {
        'nested-open-owner': ('''public struct Inner<T> { public T Value; }
public struct Outer<T> { public T Identity<U>(T value, U ignored) { return value; } }
public struct Wrapper<T> { public int Run(T value) {
 Inner<T> inner; inner.Value = value; Outer<Inner<T>> outer;
 Inner<T> result = outer.Identity<int>(inner, 1); return (int)result.Value; } }
class Program { public static int Main() { Wrapper<int> a; Wrapper<long> b;
 long second = 22; return a.Run(20) + b.Run(second); } }''', True),
        'ordinary-own-explicit': ('''public struct Own {
 public U Identity<U>(U value) { return value; }
 public int Run() { return this.Identity<int>(42); } }
class Program { public static int Main() { Own x; return x.Run(); } }''', True),
        'own-two-types': ('''public struct Own<T> {
 public T Identity<U>(T value, U ignored) { return value; }
 public T Run(T value) { return Identity<int>(value, 1); } }
class Program { public static int Main() { Own<int> a; Own<long> b;
 long second = 22; return a.Run(20) + (int)b.Run(second); } }''', True),
        'own-two-types-inferred': ('''public struct Own<T> {
 public T Identity<U>(T value, U ignored) { return value; }
 public T Run(T value) { return Identity(value, 1); } }
class Program { public static int Main() { Own<int> a; Own<long> b;
 long second = 22; return a.Run(20) + (int)b.Run(second); } }''', True),
        'own-two-types-this-inferred': ('''public struct Own<T> {
 public T Identity<U>(T value, U ignored) { return value; }
 public T Run(T value) { return this.Identity(value, 1); } }
class Program { public static int Main() { Own<int> a; Own<long> b;
 long second = 22; return a.Run(20) + (int)b.Run(second); } }''', True),
        'own-overload': ('''public struct Own<T> {
 public U Identity<U>(U value) { return value; }
 public U Identity<U>(U[] values) { return values[0]; }
 public int Run() { int[] a = new int[1]; a[0] = 42;
  int value = this.Identity<int>(a); delete[] a; return value; } }
class Program { public static int Main() { Own<int> x; return x.Run(); } }''', True),
        'own-recursive': ('''public struct Own<T> {
 public U Identity<U>(U value, int n) { if (n == 0) return value; return this.Identity<U>(value, n - 1); }
 public int Run() { return this.Identity<int>(42, 2); } }
class Program { public static int Main() { Own<int> x; return x.Run(); } }''', True),
        'own-shared-demand': ('''public struct Own<T> {
 public U Identity<U>(U value) { return value; }
 public int Run() { return this.Identity<int>(21); } }
class Program { public static int Main() { Own<int> x;
 return x.Identity<int>(21) + x.Run(); } }''', True),
        'own-const-explicit': ('''public struct Own<const int N> {
 public int Sum<const int M>() { return N + M; }
 public int Run() { return this.Sum<22>(); } }
class Program { public static int Main() { Own<20> x; return x.Run(); } }''', True),
        'own-value-pack': ('''public struct Own<const int N> {
 public int Sum<pack const int Ns>() { return N + packsum(Ns); }
 public int Run() { return this.Sum<10, 12>(); } }
class Program { public static int Main() { Own<20> x; return x.Run(); } }''', True),
        'own-value-pack-invalid': ('''public struct Own<const int N> {
 public int At<pack const int Ns>() { return N + packat(Ns, 2); }
 public int Run() { return this.At<10, 12>(); } }
class Program { public static int Main() { Own<20> x; return x.Run(); } }''', False),
        'own-dependent-pack-valid': ('''public struct Own<const int N> {
 public int At<pack const int Ns>() { return packat(Ns, N); }
 public int Run() { return this.At<20, 42>(); } }
class Program { public static int Main() { Own<1> x; return x.Run(); } }''', True),
        'own-dependent-pack-invalid': ('''public struct Own<const int N> {
 public int At<pack const int Ns>() { return packat(Ns, N); }
 public int Run() { return this.At<20, 42>(); } }
class Program { public static int Main() { Own<2> x; return x.Run(); } }''', False),
        'own-dependent-pack-unused': ('''public struct Own<const int N> {
 public int At<pack const int Ns>() { return packat(Ns, N); } }
class Program { public static int Main() { Own<1> x; Own<2> unused;
 return x.At<20, 42>(); } }''', True),
        'own-dependent-pack-array-element': ('''public struct Own<const int N> {
 public int At<pack const int Ns>() { return packat(Ns, N); } }
class Program { public static int Main() { Own<1>[] items = new Own<1>[1]; Own<2> unused;
 return items[0].At<20, 42>(); } }''', True),
        'own-dependent-pack-array-element-invalid': ('''public struct Own<const int N> {
 public int At<pack const int Ns>() { return packat(Ns, N); } }
class Program { public static int Main() { Own<2>[] items = new Own<2>[1];
 return items[0].At<20, 42>(); } }''', False),
        'own-dependent-pack-array-two-owners': ('''public struct Own<const int N> {
 public int At<pack const int Ns>() { return packat(Ns, N); } }
class Program { public static int Main() { Own<0>[] a = new Own<0>[1]; Own<1>[] b = new Own<1>[1];
 return a[0].At<20, 22>() + b[0].At<20, 22>(); } }''', True),
        'own-dependent-pack-two-demands': ('''public struct Own<const int N> {
 public int At<pack const int Ns>() { return packat(Ns, N); } }
class Program { public static int Main() { Own<0> a; Own<1> b;
 return a.At<20, 22>() + b.At<20, 22>(); } }''', True),
        'own-static-dependent-pack': ('''public struct Own<const int N> {
 public static int At<pack const int Ns>() { return packat(Ns, N); } }
class Program { public static int Main() { return Own<1>.At<20, 42>(); } }''', True),
        'own-computed-dependent-pack': ('''public struct Own<const int N> {
 public int At<pack const int Ns>() { return packat(Ns, N); } }
class Program { static Own<1> Make() { Own<1> x; return x; }
 public static int Main() { return Make().At<20, 42>(); } }''', True),
        'own-computed-dependent-pack-invalid': ('''public struct Own<const int N> {
 public int At<pack const int Ns>() { return packat(Ns, N); } }
class Program { static Own<2> Make() { Own<2> x; return x; }
 public static int Main() { return Make().At<20, 42>(); } }''', False),
        'own-field-dependent-pack': ('''public struct Own<const int N> {
 public int At<pack const int Ns>() { return packat(Ns, N); } }
class Wrapper { public Own<1> value; }
class Program { public static int Main() { Wrapper w = new Wrapper();
 return w.value.At<20, 42>(); } }''', True),
        'own-inherited-field': ('''public struct Own<const int N> {
 public int At<pack const int Ns>() { return packat(Ns, N); } }
class Base { public Own<1> value; }
class Derived : Base { }
class Program { public static int Main() { Derived w = new Derived();
 return w.value.At<20, 42>(); } }''', True),
        'own-inherited-generic-base': ('''public struct Own<const int N> {
 public int At<pack const int Ns>() { return packat(Ns, N); } }
class Base<const int N> { public Own<N> value; }
class Derived : Base<1> { }
class Program { public static int Main() { Derived w = new Derived();
 return w.value.At<20, 42>(); } }''', True),
        'own-inherited-generic-chain': ('''public struct Own<const int N> {
 public int At<pack const int Ns>() { return packat(Ns, N); } }
class Base<const int N> { public Own<N> value; }
class Derived<const int M> : Base<M> { }
class Program { public static int Main() { Derived<1> w = new Derived<1>();
 Derived<2> unused = new Derived<2>(); return w.value.At<20, 42>(); } }''', True),
        'own-inherited-generic-chain-two': ('''public struct Own<const int N> {
 public int At<pack const int Ns>() { return packat(Ns, N); } }
class Base<const int N> { public Own<N> value; }
class Derived<const int M> : Base<M> { }
class Program { public static int Main() { Derived<0> a = new Derived<0>(); Derived<1> b = new Derived<1>();
 return a.value.At<20, 22>() + b.value.At<20, 22>(); } }''', True),
        'own-inherited-generic-chain-invalid': ('''public struct Own<const int N> {
 public int At<pack const int Ns>() { return packat(Ns, N); } }
class Base<const int N> { public Own<N> value; }
class Derived<const int M> : Base<M> { }
class Program { public static int Main() { Derived<2> w = new Derived<2>();
 return w.value.At<20, 42>(); } }''', False),
        'own-inherited-field-alias': ('''namespace First {
 public struct Own<const int N> { public int At<pack const int Ns>() { return packat(Ns, N); } }
 public class Base { public Own<1> value; } }
namespace Second { public class Base { } }
namespace Middle { using Pick = First; public class Mid : Pick.Base { } public class Derived : Mid { } }
namespace Consumer { using Pick = Second; class Program { public static int Main() {
 Middle.Derived w = new Middle.Derived(); return w.value.At<20, 42>(); } } }''', True),
        'own-inherited-field-invalid': ('''public struct Own<const int N> {
 public int At<pack const int Ns>() { return packat(Ns, N); } }
class Base { public Own<2> value; }
class Derived : Base { }
class Program { public static int Main() { Derived w = new Derived();
 return w.value.At<20, 42>(); } }''', False),
        'own-generic-field-dependent-pack': ('''public struct Own<const int N> {
 public int At<pack const int Ns>() { return packat(Ns, N); } }
class Wrapper<const int N> { public Own<N> value; }
class Program { public static int Main() { Wrapper<1> w = new Wrapper<1>();
 Wrapper<2> unused = new Wrapper<2>(); return w.value.At<20, 42>(); } }''', True),
        'own-generic-field-array-receiver': ('''public struct Own<const int N> {
 public int At<pack const int Ns>() { return packat(Ns, N); } }
class Wrapper<const int N> { public Own<N> value; }
class Program { public static int Main() {
 Wrapper<1>[] items = new Wrapper<1>[1]; items[0] = new Wrapper<1>();
 Wrapper<2> unused = new Wrapper<2>(); return items[0].value.At<20, 42>(); } }''', True),
        'own-generic-field-array-two-owners': ('''public struct Own<const int N> {
 public int At<pack const int Ns>() { return packat(Ns, N); } }
class Wrapper<const int N> { public Own<N> value; }
class Program { public static int Main() {
 Wrapper<0>[] a = new Wrapper<0>[1]; a[0] = new Wrapper<0>();
 Wrapper<1>[] b = new Wrapper<1>[1]; b[0] = new Wrapper<1>();
 return a[0].value.At<20, 22>() + b[0].value.At<20, 22>(); } }''', True),
        'own-generic-field-array-invalid': ('''public struct Own<const int N> {
 public int At<pack const int Ns>() { return packat(Ns, N); } }
class Wrapper<const int N> { public Own<N> value; }
class Program { public static int Main() {
 Wrapper<2>[] items = new Wrapper<2>[1]; items[0] = new Wrapper<2>();
 return items[0].value.At<20, 42>(); } }''', False),
        'own-generic-field-dependent-pack-invalid': ('''public struct Own<const int N> {
 public int At<pack const int Ns>() { return packat(Ns, N); } }
class Wrapper<const int N> { public Own<N> value; }
class Program { public static int Main() { Wrapper<2> w = new Wrapper<2>();
 return w.value.At<20, 42>(); } }''', False),
        'own-generic-field-two-owners': ('''public struct Own<const int N> {
 public int At<pack const int Ns>() { return packat(Ns, N); } }
class Wrapper<const int N> { public Own<N> value; }
class Program { public static int Main() { Wrapper<0> a = new Wrapper<0>(); Wrapper<1> b = new Wrapper<1>();
 return a.value.At<20, 22>() + b.value.At<20, 22>(); } }''', True),
        'own-generic-field-container-pack': ('''public struct Own<const int N> {
 public int At<pack const int Ns>() { return packat(Ns, N); } }
class Wrapper<const int N, pack Ts> { public Own<N> value; }
class Program { public static int Main() { Wrapper<1, int, long> w = new Wrapper<1, int, long>();
 Wrapper<2, int> unused = new Wrapper<2, int>(); return w.value.At<20, 42>(); } }''', True),
        'own-generic-field-packtype': ('''public struct Own<const int N> {
 public int At<pack const int Ns>() { return packat(Ns, N); } }
class Wrapper<pack Ts> { public packtype<Ts, 0> value; }
class Program { public static int Main() { Wrapper<Own<1>, int> w = new Wrapper<Own<1>, int>();
 Wrapper<Own<2>> unused = new Wrapper<Own<2>>(); return w.value.At<20, 42>(); } }''', True),
        'own-field-qualified-dependent-pack': ('''namespace Nested {
 public struct Own<const int N> { public int At<pack const int Ns>() { return packat(Ns, N); } }
 public class Wrapper { public Own<1> value; } }
class Program { public static int Main() { Nested.Wrapper w = new Nested.Wrapper();
 return w.value.At<20, 42>(); } }''', True),
        'own-field-alias-scope': ('''namespace First {
 public struct Own<const int N> { public int At<pack const int Ns>() { return packat(Ns, N); } } }
namespace Second {
 public struct Own<const int N> { public int At<pack const int Ns>() { return 7; } } }
namespace Producer { using Pick = First; public class Wrapper { public Pick.Own<1> value; } }
namespace Consumer { using Pick = Second; class Program {
 public static int Main() { Producer.Wrapper w = new Producer.Wrapper();
  return w.value.At<20, 42>(); } } }''', True),
        'owner-return-chain': ('''class Item { public int Value; }
public struct Own<T> { public T Give<U>(T value, U ignored) { return value; } }
class Program { public static int Main() { Own<Item> x; Item item = new Item(); item.Value = 42;
 return x.Give(item, 1).Value; } }''', True),
        'own-explicit': ('''public struct Own<T> {
 public U Identity<U>(U value) { return value; }
 public int Run() { return Identity<int>(42); } }
class Program { public static int Main() { Own<int> x; return x.Run(); } }''', True),
        'own-inferred': ('''public struct Own<T> {
 public U Identity<U>(U value) { return value; }
 public int Run() { return Identity(42); } }
class Program { public static int Main() { Own<int> x; return x.Run(); } }''', True),
        'own-this-explicit': ('''public struct Own<T> {
 public U Identity<U>(U value) { return value; }
 public int Run() { return this.Identity<int>(42); } }
class Program { public static int Main() { Own<int> x; return x.Run(); } }''', True),
        'own-this-inferred': ('''public struct Own<T> {
 public U Identity<U>(U value) { return value; }
 public int Run() { return this.Identity(42); } }
class Program { public static int Main() { Own<int> x; return x.Run(); } }''', True),
        'static-explicit': ('''class Program { public static int Main() { return Alpha.Box<int>.Static<int>(42); } }''', True),
        'static-inferred': ('''class Program { public static int Main() { return Alpha.Box<int>.Static(42); } }''', True),
        'explicit': ('''class Program { public static int Main() {
 Alpha.Box<int> a; Beta.Box<int> b; int x = a.Echo<int>(42);
 if (Trace.Which != 1 || x != 42) return 1;
 int y = b.Echo<int>(42); return Trace.Which == 2 && y == 42 ? 42 : 2;
} }''', True),
        'inferred': ('''class Program { public static int Main() {
 Alpha.Box<int> a; Beta.Box<int> b; int x = a.Echo(42);
 if (Trace.Which != 1 || x != 42) return 1;
 int y = b.Echo(42); return Trace.Which == 2 && y == 42 ? 42 : 2;
} }''', True),
        'overload-explicit': ('''class Program { public static int Main() {
 Alpha.Box<int> a; int[] values = new int[1]; values[0] = 42;
 int x = a.Echo<int>(values); delete[] values;
 return Trace.Which == 3 && x == 42 ? 42 : 3;
} }''', True),
        'overload-inferred': ('''class Program { public static int Main() {
 Alpha.Box<int> a; int[] values = new int[1]; values[0] = 42;
 int x = a.Echo(values); delete[] values;
 return Trace.Which == 3 && x == 42 ? 42 : 4;
} }''', True),
        'constraint-valid': ('''class Program { public static int Main() {
 Alpha.Box<int> a; Payload value = new Payload(); int x = a.ClassOnly<Payload>(value);
 return x;
} }''', True),
        'constraint-invalid': ('''class Program { public static int Main() {
 Alpha.Box<int> a; return a.ClassOnly<int>(42);
} }''', False),
        'dependent-constraint-valid': ('''public trait Reducer<T> { T Combine(T left, T right); }
public struct IntReducer { }
impl Reducer<int> for IntReducer { int Combine(int left, int right) { return left + right; } }
public struct Holder<T> { public T Keep<TOp>(T value, TOp op) where TOp : Reducer<T> { return value; } }
class Program { public static int Main() { Holder<int> a; IntReducer op; return a.Keep<IntReducer>(42, op); } }''', True),
        'dependent-constraint-invalid': ('''public trait Reducer<T> { T Combine(T left, T right); }
public struct IntReducer { }
impl Reducer<int> for IntReducer { int Combine(int left, int right) { return left + right; } }
public struct Holder<T> { public T Keep<TOp>(T value, TOp op) where TOp : Reducer<T> { return value; } }
class Program { public static int Main() { Holder<long> a; IntReducer op; return (int)a.Keep<IntReducer>(42, op); } }''', False),
        'dependent-constraint-inferred-valid': ('''public trait Reducer<T> { T Combine(T left, T right); }
public struct IntReducer { }
impl Reducer<int> for IntReducer { int Combine(int left, int right) { return left + right; } }
public struct Holder<T> { public T Keep<TOp>(T value, TOp op) where TOp : Reducer<T> { return value; } }
class Program { public static int Main() { Holder<int> a; IntReducer op; return a.Keep(42, op); } }''', True),
        'dependent-constraint-inferred-invalid': ('''public trait Reducer<T> { T Combine(T left, T right); }
public struct IntReducer { }
impl Reducer<int> for IntReducer { int Combine(int left, int right) { return left + right; } }
public struct Holder<T> { public T Keep<TOp>(T value, TOp op) where TOp : Reducer<T> { return value; } }
class Program { public static int Main() { Holder<long> a; IntReducer op; long value = 42; return (int)a.Keep(value, op); } }''', False),
        'dependent-constraint-mixed-invalid': ('''public trait Reducer<T> { T Combine(T left, T right); }
public struct IntReducer { }
impl Reducer<int> for IntReducer { int Combine(int left, int right) { return left + right; } }
public struct Holder<T> { public T Keep<TOp>(T value, TOp op) where TOp : Reducer<T> { return value; } }
class Program { public static int Main() { Holder<int> a; Holder<long> b; IntReducer op;
 int x = a.Keep<IntReducer>(42, op); return x + (int)b.Keep<IntReducer>(42, op); } }''', False),
        'dependent-constraint-unused-owner': ('''public trait Reducer<T> { T Combine(T left, T right); }
public struct IntReducer { }
impl Reducer<int> for IntReducer { int Combine(int left, int right) { return left + right; } }
public struct Holder<T> { public T Keep<TOp>(T value, TOp op) where TOp : Reducer<T> { return value; } }
class Program { public static int Main() { Holder<int> a; Holder<long> b; IntReducer op;
 return a.Keep<IntReducer>(42, op); } }''', True),
        'computed-explicit': ('''class Program { static Alpha.Box<int> Make() { Alpha.Box<int> a; return a; }
 public static int Main() { return Make().Echo<int>(42); } }''', True),
        'computed-inferred': ('''class Program { static Alpha.Box<int> Make() { Alpha.Box<int> a; return a; }
 public static int Main() { return Make().Echo(42); } }''', True),
        'computed-other-inferred': ('''class Program { static Beta.Box<int> Make() { Beta.Box<int> b; return b; }
 public static int Main() { int value = Make().Echo(42); return Trace.Which == 2 ? value : 1; } }''', True),
        'computed-wrong-inferred': ('''namespace Gamma { public struct Box<T> { } }
class Program { static Gamma.Box<int> Make() { Gamma.Box<int> value; return value; }
 public static int Main() { return Make().Echo(42); } }''', False),
        'wrong-owner-explicit': ('''namespace Gamma { public struct Box<T> { } }
class Program { public static int Main() { Gamma.Box<int> value; return value.Echo<int>(42); } }''', False),
        'wrong-owner-inferred': ('''namespace Gamma { public struct Box<T> { } }
class Program { public static int Main() { Gamma.Box<int> value; return value.Echo(42); } }''', False),
    }
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-owner-method-inference-') as temporary:
        root = Path(temporary)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>OwnerInference</Name></PropertyGroup>'
                           '<ItemGroup><Compile Include="Main.ns"/></ItemGroup></Project>', encoding='utf-8')
        for name, (body, expected) in cases.items():
            (root / 'Main.ns').write_text(prefix + body, encoding='utf-8')
            for level in ('O0', 'O2'):
                exe = root / (name + '-' + level + '.exe')
                result = run([compiler, '--emit-project-oop-exe', project, '-' + level, '-o', exe,
                              '--no-artifact-cache'], root)
                if expected:
                    if result.returncode or run([exe], root).returncode != 42:
                        failures.append((name, level, 'expected execution'))
                else:
                    diagnostic = ('outside a pack of length 2' if name in ('own-value-pack-invalid', 'own-dependent-pack-invalid', 'own-computed-dependent-pack-invalid', 'own-dependent-pack-array-element-invalid', 'own-generic-field-dependent-pack-invalid', 'own-generic-field-array-invalid', 'own-inherited-field-invalid', 'own-inherited-generic-chain-invalid') else
                                  'does not implement it' if name.startswith('dependent-constraint-') else
                                  'requires a reference type' if name == 'constraint-invalid' else
                                  'has no visible Stage 44 generic method template'
                                  if name.endswith('explicit') else 'has no compatible member named')
                    if not result.returncode or diagnostic not in result.stdout + result.stderr:
                        failures.append((name, level, 'expected receiver rejection'))
    if failures:
        print('FAIL', failures, file=sys.stderr, flush=True)
    return int(bool(failures))


if __name__ == '__main__':
    sys.exit(main())
