"""Resolve generic method owners returned by ordinary user indexers."""
from pathlib import Path
import subprocess
import sys
import tempfile


def run(command, root):
    command = [str(value) for value in command]
    result = subprocess.run(command, cwd=root, capture_output=True, text=True,
                            errors='replace', timeout=120)
    print('COMMAND', subprocess.list2cmdline(command), 'EXIT', result.returncode,
          result.stdout + result.stderr, flush=True)
    return result


def main():
    compiler = Path(sys.argv[1]).resolve()
    producer_source = '''namespace First { public class Key { } }
namespace Second { public class Key { } }
namespace Fixture { using Pick = First;
public struct Own<const int N> { public int At<pack const int Ns>() { return packat(Ns, N); } }
public class Lookup0 { public Own<0> this[int index] { get { Own<0> value; return value; } } }
public class Lookup1 { public Own<1> this[int index] { get { Own<1> value; return value; } } }
public class Lookup2 { public Own<2> this[int index] { get { Own<2> value; return value; } } }
public struct StructLookup { public Own<1> this[int index] { get { Own<1> value; return value; } } }
public class GenericLookup<const int N> { public Own<N> this[int index] { get { Own<N> value; return value; } } }
public struct GenericStruct<const int N> { public Own<N> this[int index] { get { Own<N> value; return value; } } }
public class PackedLookup<const int N, pack Ts> { public Own<N> this[int index] { get { Own<N> value; return value; } } }
public class Overloaded {
 public Own<0> this[int index] { get { Own<0> value; return value; } }
 public Own<1> this[long index] { get { Own<1> value; return value; } } }
public class GenericOverloaded<const int N> {
 public Own<0> this[int index] { get { Own<0> value; return value; } }
 public Own<N> this[long index] { get { Own<N> value; return value; } } }
public class BaseLookup { public Own<1> this[int index] { get { Own<1> value; return value; } } }
public class DerivedLookup : BaseLookup { }
public class GenericBase<const int N> { public Own<N> this[int index] { get { Own<N> value; return value; } } }
public class GenericDerived<const int N> : GenericBase<N> { }
public class NominalOverloaded {
 public Own<0> this[Pick.Key index] { get { Own<0> value; return value; } }
 public Own<1> this[Second.Key index] { get { Own<1> value; return value; } } }
public class NumericOverloaded {
 public Own<0> this[long index] { get { Own<0> value; return value; } }
 public Own<1> this[double index] { get { Own<1> value; return value; } } }
public class Ambiguous {
 public Own<0> this[short index] { get { Own<0> value; return value; } }
 public Own<1> this[ushort index] { get { Own<1> value; return value; } } }
public class ConstantIndex {
 public Own<0> this[byte index] { get { Own<0> value; return value; } }
 public Own<1> this[long index] { get { Own<1> value; return value; } } }
public class AmbiguousPair {
 public Own<0> this[int a, long b] { get { Own<0> value; return value; } }
 public Own<1> this[long a, int b] { get { Own<1> value; return value; } } }
public class PrivateCandidate {
 public Own<0> this[short index] { get { Own<0> value; return value; } }
 private Own<1> this[ushort index] { get { Own<1> value; return value; } }
 public int PrivateControl() { ushort index = 0; Own<1> value = this[index]; return value.At<0, 42>(); }
 public int PrivateDirect() { PrivateCandidate self = this; ushort index = 0; return self[index].At<0, 42>(); } }
public class PrivateOnly { private Own<1> this[ushort index] { get { Own<1> value; return value; } } }
}
namespace Wrong { public struct Own<const int N> { public int At<pack const int Ns>() { return 7; } } }
namespace Holder { using Pick = Fixture;
 public class AliasLookup { public Pick.Own<1> this[int index] { get { Pick.Own<1> value; return value; } } }
}'''
    consumers = {
        'one': ('''using Fixture;
class Program { public static int Main() { Lookup1 items = new Lookup1(); Own<2> unused;
 return items[0].At<20, 42>(); } }''', True),
        'two': ('''using Fixture;
class Program { public static int Main() { Lookup0 a = new Lookup0(); Lookup1 b = new Lookup1();
 return a[0].At<20, 22>() + b[0].At<20, 22>(); } }''', True),
        'invalid': ('''using Fixture;
class Program { public static int Main() { Lookup2 items = new Lookup2();
 return items[0].At<20, 42>(); } }''', False),
        'struct': ('''using Fixture;
class Program { public static int Main() { StructLookup items; Own<2> unused;
 return items[0].At<20, 42>(); } }''', True),
        'alias': ('''using Pick = Wrong;
class Program { public static int Main() { Holder.AliasLookup items = new Holder.AliasLookup();
 return items[0].At<20, 42>(); } }''', True),
        'generic': ('''using Fixture;
class Program { public static int Main() { GenericLookup<1> items = new GenericLookup<1>(); Own<2> unused;
 return items[0].At<20, 42>(); } }''', True),
        'generic_struct': ('''using Fixture;
class Program { public static int Main() { GenericStruct<1> items; Own<2> unused;
 return items[0].At<20, 42>(); } }''', True),
        'generic_pack': ('''using Fixture;
class Program { public static int Main() { PackedLookup<1, int> items = new PackedLookup<1, int>();
 Own<2> unused; return items[0].At<20, 42>(); } }''', True),
        'generic_invalid': ('''using Fixture;
class Program { public static int Main() { GenericLookup<2> items = new GenericLookup<2>();
 return items[0].At<20, 42>(); } }''', False),
        'overloaded': ('''using Fixture;
class Program { public static int Main() { Overloaded items = new Overloaded(); long index = 0;
 return items[0].At<20, 22>() + items[index].At<20, 22>(); } }''', True),
        'generic_overloaded': ('''using Fixture;
class Program { public static int Main() { GenericOverloaded<1> items = new GenericOverloaded<1>();
 long index = 0; return items[0].At<20, 22>() + items[index].At<20, 22>(); } }''', True),
        'inherited': ('''using Fixture;
class Program { public static int Main() { DerivedLookup items = new DerivedLookup(); Own<2> unused;
 return items[0].At<20, 42>(); } }''', True),
        'generic_inherited': ('''using Fixture;
class Program { public static int Main() { GenericDerived<1> items = new GenericDerived<1>(); Own<2> unused;
 return items[0].At<20, 42>(); } }''', True),
        'nominal': ('''using Fixture; using Pick = Second;
class Program { public static int Main() { NominalOverloaded items = new NominalOverloaded();
 First.Key a = new First.Key(); Pick.Key b = new Pick.Key();
 return items[a].At<20, 22>() + items[b].At<20, 22>(); } }''', True),
        'conversion': ('''using Fixture;
class Program { public static int Main() { NumericOverloaded items = new NumericOverloaded(); int index = 0;
 return items[index].At<42, 0>(); } }''', True),
        'ambiguous_control': ('''using Fixture;
class Program { public static int Main() { Ambiguous items = new Ambiguous(); byte index = 0;
 Own<0> selected = items[index]; return selected.At<42, 0>(); } }''', False),
        'ambiguous_direct': ('''using Fixture;
class Program { public static int Main() { Ambiguous items = new Ambiguous(); byte index = 0;
 return items[index].At<42, 0>(); } }''', False),
        'constant': ('''using Fixture;
class Program { public static int Main() { ConstantIndex items = new ConstantIndex();
 return items[1].At<42, 0>(); } }''', True),
        'constant_outside': ('''using Fixture;
class Program { public static int Main() { ConstantIndex items = new ConstantIndex();
 return items[256].At<20, 42>(); } }''', True),
        'constant_local': ('''using Fixture;
class Program { public static int Main() { ConstantIndex items = new ConstantIndex(); const int index = 1;
 return items[index].At<20, 42>(); } }''', True),
        'constant_local_negative': ('''using Fixture;
class Program { public static int Main() { ConstantIndex items = new ConstantIndex(); const int index = -1;
 return items[index].At<20, 42>(); } }''', True),
        'numeric_parameter': ('''using Fixture;
class Program { public static int Run(int index) { ConstantIndex items = new ConstantIndex();
 return items[index].At<20, 42>(); } public static int Main() { return Run(1); } }''', True),
        'ambiguous_pair_control': ('''using Fixture;
class Program { public static int Main() { AmbiguousPair items = new AmbiguousPair(); int a = 0; int b = 0;
 Own<0> selected = items[a, b]; return selected.At<42, 0>(); } }''', False),
        'ambiguous_pair_direct': ('''using Fixture;
class Program { public static int Main() { AmbiguousPair items = new AmbiguousPair(); int a = 0; int b = 0;
 return items[a, b].At<42, 0>(); } }''', False),
        'private_control': ('''using Fixture;
class Program { public static int Main() { PrivateCandidate items = new PrivateCandidate(); byte index = 0;
 Own<0> selected = items[index]; return selected.At<42, 0>(); } }''', True),
        'private_direct': ('''using Fixture;
class Program { public static int Main() { PrivateCandidate items = new PrivateCandidate(); byte index = 0;
 return items[index].At<42, 0>(); } }''', True),
        'private_self': ('''using Fixture;
class Program { public static int Main() { PrivateCandidate items = new PrivateCandidate();
 return items.PrivateControl() + items.PrivateDirect() - 42; } }''', True),
        'private_only': ('''using Fixture;
class Program { public static int Main() { PrivateOnly items = new PrivateOnly(); ushort index = 0;
 return items[index].At<0, 42>(); } }''', 'inaccessible'),
        'private_exact': ('''using Fixture;
class Program { public static int Main() { PrivateCandidate items = new PrivateCandidate(); ushort index = 0;
 return items[index].At<0, 42>(); } }''', 'inaccessible'),
        'private_same_name': ('''using Fixture;
namespace Other { class PrivateCandidate { public static int Main() {
 Fixture.PrivateCandidate items = new Fixture.PrivateCandidate(); byte index = 0;
 return items[index].At<42, 0>(); } } }''', True),
    }
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-owner-indexer-') as temporary:
        root = Path(temporary)
        (root / 'Producer.ns').write_text(producer_source, encoding='utf-8')
        project = root / 'Consumer.nsproj'

        def exercise(label, reference):
            project.write_text('<Project><PropertyGroup><Name>OwnerIndexer</Name></PropertyGroup><ItemGroup>'
                               + reference + '<Compile Include="Consumer.ns"/></ItemGroup></Project>',
                               encoding='utf-8')
            for name, (text, expected) in consumers.items():
                (root / 'Consumer.ns').write_text(text, encoding='utf-8')
                for level in ('O0', 'O2'):
                    exe = root / (label + '-' + name + '-' + level + '.exe')
                    build = run([compiler, '--emit-project-oop-exe', project, '-' + level, '-o', exe,
                                 '--no-artifact-cache', '--no-error-log'], root)
                    if expected == 'inaccessible':
                        if not build.returncode:
                            failures.append((label, name, level, 'private indexer accepted'))
                    elif expected:
                        if build.returncode or run([exe], root).returncode != 42:
                            failures.append((label, name, level, 'expected execution'))
                    elif name in ('ambiguous_control', 'ambiguous_pair_control'):
                        if not build.returncode or 'ambiguous' not in build.stdout + build.stderr:
                            failures.append((label, name, level, 'expected ambiguous indexer'))
                    elif name in ('ambiguous_direct', 'ambiguous_pair_direct'):
                        if not build.returncode or 'ambiguous indexer call' not in build.stdout + build.stderr:
                            failures.append((label, name, level, 'expected ambiguous indexer'))
                    elif not build.returncode or 'outside a pack of length 2' not in build.stdout + build.stderr:
                        failures.append((label, name, level, 'expected invalid index'))

        exercise('source', '<Compile Include="Producer.ns"/>')
        producer = root / 'Producer.nsproj'
        for dynamic in (False, True):
            producer.write_text('<Project><PropertyGroup><Name>IndexerProducer</Name><OutputType>' +
                                ('SharedLibrary' if dynamic else 'StaticLibrary') +
                                '</OutputType></PropertyGroup><ItemGroup><Compile Include="Producer.ns"/>'
                                '</ItemGroup></Project>', encoding='utf-8')
            for level in ('O0', 'O2'):
                stem = ('dynamic-' if dynamic else 'static-') + level
                library = root / (stem + '.lib')
                output = library.with_suffix('.dll') if dynamic else library
                emit = '--emit-project-oop-dll' if dynamic else '--emit-project-oop-lib'
                if run([compiler, emit, producer, '-' + level, '-o', output,
                        '--no-artifact-cache', '--no-error-log'], root).returncode:
                    failures.append((stem, 'producer'))
                    continue
                reference = '<LibraryReference Include="' + library.as_posix() + '" Metadata="' + \
                            library.with_suffix('.nsmeta').as_posix() + '"/>'
                exercise(stem, reference)
    if failures:
        print('FAIL', failures, file=sys.stderr, flush=True)
    return int(bool(failures))


if __name__ == '__main__':
    sys.exit(main())
