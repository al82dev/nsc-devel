"""Exercise generic-owner methods across static and dynamic NativeSharp libraries."""
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
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-owner-method-binary-') as temporary:
        root = Path(temporary)
        (root / 'Producer.ns').write_text('''namespace Other { public class Value { public int Number; } }
namespace Fixture {
public struct Pair<const int A, const int B> { public int Sum() { return A + B; } }
public struct Box<const int N> {
 public Pair<N, M> Make<const int M>() { Pair<N, M> p; return p; }
 public U Echo<U>(U value) { return value; }
 public U Echo<U>(U[] values) { return values[0]; }
 public static U Static<U>(U value) { return value; }
 public int Internal() { return Echo<int>(42); }
}
public class Box { public U Echo<U>(U value) { return value; } }
public struct PackOwner<const int N> { public int At<pack const int Ns>() { return packat(Ns, N); } }
public class GenericWrapper<const int N> { public PackOwner<N> value; }
public class PackedWrapper<const int N, pack Ts> { public PackOwner<N> value; }
public class TypePackedWrapper<pack Ts> { public packtype<Ts, 0> value; }
public class BaseWrapper { public PackOwner<1> value; }
public class DerivedWrapper : BaseWrapper { }
public class GenericBase<const int N> { public PackOwner<N> value; }
public class GenericDerived<const int N> : GenericBase<N> { }
public struct TypeOwner<T> { public T Echo<U>(T value, U ignored) { return value; } }
public class Value { public int Number; }
public struct Nest<T> { public T Unwrap(T value) { return value; } }
public class Wrapper { public PackOwner<1> value; public TypeOwner<int> typed; public TypeOwner<Value> named; public TypeOwner<Pair<20, 22>> nested; public TypeOwner<Nest<Value>> nestedNamed; public TypeOwner<Nest<Value>[]> nestedArray; public TypeOwner<Value[]> namedArray; public TypeOwner<int[]> primitiveArray; }
public trait Reducer<T> { T Combine(T left, T right); }
public struct Holder<T> { public T Keep<TOp>(T value, TOp op) where TOp : Reducer<T> { return value; } }
}''', encoding='utf-8')
        valid_source = '''using Fixture;
public struct IntReducer { }
impl Reducer<int> for IntReducer { int Combine(int left, int right) { return left + right; } }
class Program { public static int Main() { Box<20> box; Pair<20, 22> p = box.Make<22>();
 Box plain = new Box(); int[] values = new int[1]; values[0] = p.Sum();
 int a = box.Echo<int>(values); int b = box.Echo(values); delete[] values;
 Holder<int> h; IntReducer op; int c = h.Keep(42, op);
 Wrapper w = new Wrapper(); int field = w.value.At<20, 42>();
 int typed = w.typed.Echo<int>(42, 1);
 Value named = new Value(); named.Number = 42; int namedResult = w.named.Echo<int>(named, 1).Number;
 int nestedResult = w.nested.Echo<int>(p, 1).Sum();
 Nest<Value> nv; Nest<Value> nested = w.nestedNamed.Echo<int>(nv, 1);
  Nest<Value>[] array = new Nest<Value>[1]; Nest<Value>[] arrayResult = w.nestedArray.Echo<int>(array, 1);
  Value[] namedArray = new Value[1]; Value[] namedArrayResult = w.namedArray.Echo<int>(namedArray, 1);
  int[] intArray = new int[1]; int[] intArrayResult = w.primitiveArray.Echo<int>(intArray, 1);
 GenericWrapper<1> genericWrapper = new GenericWrapper<1>();
 GenericWrapper<2> unusedWrapper = new GenericWrapper<2>();
 int genericField = genericWrapper.value.At<20, 42>();
 GenericWrapper<1>[] wrappers = new GenericWrapper<1>[1];
 wrappers[0] = new GenericWrapper<1>(); int arrayField = wrappers[0].value.At<20, 42>();
 GenericWrapper<0>[] firstWrappers = new GenericWrapper<0>[1];
 firstWrappers[0] = new GenericWrapper<0>();
 int twoArrayOwners = firstWrappers[0].value.At<20, 22>() + wrappers[0].value.At<20, 22>();
 PackedWrapper<1, int> packedWrapper = new PackedWrapper<1, int>();
 TypePackedWrapper<PackOwner<1>> typePacked = new TypePackedWrapper<PackOwner<1>>();
 int packedField = packedWrapper.value.At<20, 42>(); int typePackedField = typePacked.value.At<20, 42>();
 DerivedWrapper inherited = new DerivedWrapper(); int inheritedField = inherited.value.At<20, 42>();
 GenericDerived<1> inheritedGeneric = new GenericDerived<1>();
 int inheritedGenericField = inheritedGeneric.value.At<20, 42>();
 return plain.Echo(a) == 42 && b == 42 && c == 42 && Box<20>.Static(42) == 42 && box.Internal() == 42 && field == 42 && typed == 42 && namedResult == 42 && nestedResult == 42 && genericField == 42 && arrayField == 42 && twoArrayOwners == 42 && packedField == 42 && typePackedField == 42 && inheritedField == 42 && inheritedGenericField == 42 ? 42 : 1; } }'''
        invalid_source = '''using Fixture;
public struct IntReducer { }
impl Reducer<int> for IntReducer { int Combine(int left, int right) { return left + right; } }
class Program { public static int Main() { Holder<long> h; IntReducer op; long value = 42;
 return (int)h.Keep(value, op); } }'''
        invalid_field_source = '''using Fixture;
class Program { public static int Main() { GenericWrapper<2> value = new GenericWrapper<2>();
 return value.value.At<20, 42>(); } }'''
        invalid_array_field_source = '''using Fixture;
class Program { public static int Main() { GenericWrapper<2>[] values = new GenericWrapper<2>[1];
 values[0] = new GenericWrapper<2>(); return values[0].value.At<20, 42>(); } }'''
        producer = root / 'Producer.nsproj'
        consumer = root / 'Consumer.nsproj'
        for dynamic in (False, True):
            producer.write_text('<Project><PropertyGroup><Name>OwnerProducer</Name>'
                                '<OutputType>' + ('SharedLibrary' if dynamic else 'StaticLibrary') +
                                '</OutputType></PropertyGroup><ItemGroup><Compile Include="Producer.ns"/>'
                                '</ItemGroup></Project>', encoding='utf-8')
            for producer_level in ('O0', 'O2'):
                stem = ('dynamic-' if dynamic else 'static-') + producer_level
                library = root / (stem + '.lib')
                output = library.with_suffix('.dll') if dynamic else library
                if run([compiler, '--emit-project-oop-dll' if dynamic else '--emit-project-oop-lib',
                        producer, '-' + producer_level, '-o', output, '--no-artifact-cache'], root).returncode:
                    failures.append((stem, 'producer'))
                    continue
                consumer.write_text('<Project><PropertyGroup><Name>OwnerConsumer</Name></PropertyGroup>'
                                    '<ItemGroup><Compile Include="Consumer.ns"/><LibraryReference Include="' +
                                    library.as_posix() + '" Metadata="' + library.with_suffix('.nsmeta').as_posix() +
                                    '"/></ItemGroup></Project>', encoding='utf-8')
                for level in ('O0', 'O2'):
                    exe = root / (stem + '-' + level + '.exe')
                    (root / 'Consumer.ns').write_text(valid_source, encoding='utf-8')
                    if run([compiler, '--emit-project-oop-exe', consumer, '-' + level, '-o', exe,
                            '--no-artifact-cache'], root).returncode:
                        failures.append((stem, level, 'consumer'))
                    elif run([exe], root).returncode != 42:
                        failures.append((stem, level, 'execute'))
                    (root / 'Consumer.ns').write_text(invalid_source, encoding='utf-8')
                    negative = run([compiler, '--emit-project-oop-exe', consumer, '-' + level,
                                    '-o', exe, '--no-artifact-cache'], root)
                    if not negative.returncode or 'does not implement it' not in negative.stdout + negative.stderr:
                        failures.append((stem, level, 'dependent constraint'))
                    (root / 'Consumer.ns').write_text(invalid_field_source, encoding='utf-8')
                    negative_field = run([compiler, '--emit-project-oop-exe', consumer, '-' + level,
                                          '-o', exe, '--no-artifact-cache'], root)
                    if not negative_field.returncode or 'outside a pack of length 2' not in negative_field.stdout + negative_field.stderr:
                        failures.append((stem, level, 'invalid generic field'))
                    (root / 'Consumer.ns').write_text(invalid_array_field_source, encoding='utf-8')
                    negative_array_field = run([compiler, '--emit-project-oop-exe', consumer, '-' + level,
                                                '-o', exe, '--no-artifact-cache'], root)
                    if not negative_array_field.returncode or 'outside a pack of length 2' not in negative_array_field.stdout + negative_array_field.stderr:
                        failures.append((stem, level, 'invalid array generic field'))
    if failures:
        print('FAIL', failures, file=sys.stderr, flush=True)
    return int(bool(failures))


if __name__ == '__main__':
    sys.exit(main())
