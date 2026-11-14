"""Evaluate an overloaded index once for compound accessor assignment."""
from pathlib import Path
import subprocess
import sys
import tempfile


TYPES = '''struct Key {
 public static int Hits;
 public static int operator +(Key a, Key b) { Hits = Hits + 1; return 0; }
 public static int operator -(Key a) { Hits = Hits + 1; return 0; }
}
class Container {
 public int data;
 public int lastGet; public int lastSet;
 public static int receiverHits;
 public static Container operator +(Container value, int offset) { receiverHits = receiverHits + 1; return value; }
 public int Number { get { return data; } set { data = value; } }
 public int this[int i] { get { lastGet = i; return data; } set { lastSet = i; data = value; } }
}
'''


def main():
    compiler = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix='nsc-accessor-reevaluation-') as temporary:
        root = Path(temporary)
        project = root / 'Case.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Case</Name></PropertyGroup>'
                           '<ItemGroup><Compile Include="Main.ns"/></ItemGroup></Project>', encoding='utf-8')
        for index in ('k + k', '-k', '(k + k) + 1'):
            expected = '1' if index.endswith('+ 1') else '0'
            source = TYPES + ('class Program { public static int Main() { Container c = new Container(); '
                              'Key k; c[' + index + '] += 7; return Key.Hits == 1 && c.data == 7 && '
                              'c.lastGet == ' + expected + ' && c.lastSet == ' + expected + ' ? 42 : 1; } }')
            (root / 'Main.ns').write_text(source, encoding='utf-8')
            for level in ('O0', 'O2'):
                executable = root / (level + '.exe')
                command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level,
                           '-o', str(executable), '--no-artifact-cache']
                result = subprocess.run(command, cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
                if result.returncode:
                    raise AssertionError((index, level, result.stdout + result.stderr))
                result = subprocess.run([str(executable)], cwd=root, timeout=120)
                if result.returncode != 42:
                    raise AssertionError((index, level, result.returncode))
        (root / 'Main.ns').write_text(TYPES + '''class Program {
 public static int Main() { Container c = new Container(); Key k;
  int assigned = ((c + 1)[k + k] += 7);
  return Container.receiverHits == 1 && Key.Hits == 1 && c.data == 7 && assigned == 7 ? 42 : 1; }
}''', encoding='utf-8')
        for level in ('O0', 'O2'):
            executable = root / (level + '.exe')
            command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level,
                       '-o', str(executable), '--no-artifact-cache']
            result = subprocess.run(command, cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
            if result.returncode:
                raise AssertionError(('receiver', level, result.stdout + result.stderr))
            result = subprocess.run([str(executable)], cwd=root, timeout=120)
            if result.returncode != 42:
                raise AssertionError(('receiver', level, result.returncode))
        (root / 'Main.ns').write_text(TYPES + '''class Program {
 public static int Main() { Container c = new Container();
  int assigned = ((c + 1).Number += 7);
  return Container.receiverHits == 1 && c.data == 7 && assigned == 7 ? 42 : 1; }
}''', encoding='utf-8')
        for level in ('O0', 'O2'):
            executable = root / (level + '.exe')
            command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level,
                       '-o', str(executable), '--no-artifact-cache']
            result = subprocess.run(command, cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
            if result.returncode:
                raise AssertionError(('property', level, result.stdout + result.stderr))
            result = subprocess.run([str(executable)], cwd=root, timeout=120)
            if result.returncode != 42:
                raise AssertionError(('property', level, result.returncode))
        (root / 'Main.ns').write_text('''
struct Key {
 public static int Hits;
 public static int operator +(Key a, Key b) { Hits = Hits + 1; return 3; }
}
struct WideKey {
 public static int Hits;
 public static long operator +(WideKey a, WideKey b) { Hits = Hits + 1; return 4l; }
}
class Grid {
 public int value; public int getter; public int setter;
 public int this[int a, long b] {
  get { getter = a * 10 + (int)b; return value; }
  set { setter = a * 10 + (int)b; this.value = value; }
 }
}
class Program { public static int Main() {
 Grid g = new Grid(); Key a; WideKey b;
 int result = (g[a + a, b + b] += 7);
 return Key.Hits == 1 && WideKey.Hits == 1 && g.getter == 34 && g.setter == 34 &&
        g.value == 7 && result == 7 ? 42 : 1;
} }
''', encoding='utf-8')
        for level in ('O0', 'O2'):
            executable = root / (level + '.exe')
            command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level,
                       '-o', str(executable), '--no-artifact-cache']
            result = subprocess.run(command, cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
            if result.returncode:
                raise AssertionError(('multiple indices', level, result.stdout + result.stderr))
            result = subprocess.run([str(executable)], cwd=root, timeout=120)
            if result.returncode != 42:
                raise AssertionError(('multiple indices', level, result.returncode))
        (root / 'Main.ns').write_text('''
struct Key { public static int hits; public static int operator +(Key a, Key b) { hits = hits + 1; return 3; } }
class Container { public int value; public long lastGet; public long lastSet;
 public int this[long i] { get { lastGet = i; return value; } set { lastSet = i; this.value = value; } } }
class Program { public static int Main() { Container c = new Container(); Key k; c[k + k] += 7;
 return Key.hits == 1 && c.value == 7 && c.lastGet == 3l && c.lastSet == 3l ? 42 : 1; } }
''', encoding='utf-8')
        for level in ('O0', 'O2'):
            executable = root / (level + '.exe')
            command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level,
                       '-o', str(executable), '--no-artifact-cache']
            result = subprocess.run(command, cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
            if result.returncode:
                raise AssertionError(('numeric conversion', level, result.stdout + result.stderr))
            result = subprocess.run([str(executable)], cwd=root, timeout=120)
            if result.returncode != 42:
                raise AssertionError(('numeric conversion', level, result.returncode))
        (root / 'Main.ns').write_text('''
struct Key { public static int hits; public static implicit operator int(Key a) { hits = hits + 1; return 3; } }
class Container { public int value; public int gets; public int sets;
 public int this[int i] { get { gets = i; return value; } set { sets = i; this.value = value; } } }
class Program { public static int Main() { Container c = new Container(); Key k; c[k] += 7;
 return Key.hits == 1 && c.value == 7 && c.gets == 3 && c.sets == 3 ? 42 : 1; } }
''', encoding='utf-8')
        for level in ('O0', 'O2'):
            executable = root / (level + '.exe')
            command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level,
                       '-o', str(executable), '--no-artifact-cache']
            result = subprocess.run(command, cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
            if result.returncode:
                raise AssertionError(('user conversion', level, result.stdout + result.stderr))
            result = subprocess.run([str(executable)], cwd=root, timeout=120)
            if result.returncode != 42:
                raise AssertionError(('user conversion', level, result.returncode))
        (root / 'Main.ns').write_text('''
class Key { public static int hits; public static implicit operator int(Key a) { hits = hits + 1; return 3; } }
class Container { public int value; public int gets; public int sets;
 public int this[int i] { get { gets = i; return value; } set { sets = i; this.value = value; } } }
class Program { public static int Main() { Container c = new Container(); Key k = new Key(); c[k] += 7;
 return Key.hits == 1 && c.value == 7 && c.gets == 3 && c.sets == 3 ? 42 : 1; } }
''', encoding='utf-8')
        for level in ('O0', 'O2'):
            executable = root / (level + '.exe')
            command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level,
                       '-o', str(executable), '--no-artifact-cache']
            result = subprocess.run(command, cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
            if result.returncode:
                raise AssertionError(('class conversion', level, result.stdout + result.stderr))
            result = subprocess.run([str(executable)], cwd=root, timeout=120)
            if result.returncode != 42:
                raise AssertionError(('class conversion', level, result.returncode))
        (root / 'Main.ns').write_text('''
class Key { public static int hits; public static int drops; public int value;
 public Key() { value = 3; } ~Key() { drops = drops + 1; }
 public static Key operator +(Key a, int b) { hits = hits + 1; return a; } }
class Container { public int value; public int gets; public int sets;
 public int this[Key k] { get { gets = k.value; return value; } set { sets = k.value; this.value = value; } } }
class Program { static int Work() { Container c = new Container(); Key k = new Key(); c[k + 1] += 7;
 return Key.hits == 1 && c.value == 7 && c.gets == 3 && c.sets == 3 ? 42 : 1; }
 public static int Main() { int result = Work(); return result == 42 && Key.drops == 1 ? 42 : 1; } }
''', encoding='utf-8')
        for level in ('O0', 'O2'):
            executable = root / (level + '.exe')
            command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level,
                       '-o', str(executable), '--no-artifact-cache']
            result = subprocess.run(command, cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
            if result.returncode:
                raise AssertionError(('class index', level, result.stdout + result.stderr))
            result = subprocess.run([str(executable)], cwd=root, timeout=120)
            if result.returncode != 42:
                raise AssertionError(('class index', level, result.returncode))
        (root / 'Main.ns').write_text('''
namespace NativeSharp.Core { public class Exception { } }
using NativeSharp.Core;
class Key { public static int hits; public static int drops; ~Key() { drops = drops + 1; }
 public static implicit operator int(Key a) throws Exception { hits = hits + 1; throw new Exception(); } }
class Container { public static int gets; public static int sets; public static int drops; public int value;
 ~Container() { drops = drops + 1; }
 public int this[int i] { get { gets = gets + 1; return value; } set { sets = sets + 1; this.value = value; } } }
class Program { static void Work() throws Exception { Container c = new Container(); Key k = new Key(); c[k] += 7; }
 public static int Main() { try { Work(); } catch (Exception error) {
  return Key.hits == 1 && Key.drops == 1 && Container.gets == 0 && Container.sets == 0 && Container.drops == 1 ? 42 : 1;
 } return 2; } }
''', encoding='utf-8')
        for level in ('O0', 'O2'):
            executable = root / (level + '.exe')
            command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level,
                       '-o', str(executable), '--no-artifact-cache']
            result = subprocess.run(command, cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
            if result.returncode:
                raise AssertionError(('throwing class conversion', level, result.stdout + result.stderr))
            result = subprocess.run([str(executable)], cwd=root, timeout=120)
            if result.returncode != 42:
                raise AssertionError(('throwing class conversion', level, result.returncode))
        (root / 'Main.ns').write_text('''
struct FloatKey { public static int hits; public static float operator +(FloatKey a, FloatKey b) { hits = hits + 1; return 1.5f; } }
struct DoubleKey { public static int hits; public static double operator +(DoubleKey a, DoubleKey b) { hits = hits + 1; return 2.5; } }
struct BoolKey { public static int hits; public static bool operator !(BoolKey a) { hits = hits + 1; return true; } }
struct DecimalKey { public static int hits; public static decimal operator +(DecimalKey a, DecimalKey b) { hits = hits + 1; return 3.5m; } }
class Grid {
 public int value; public int gets; public int sets;
 public int this[float a, double b, bool c, decimal d] {
  get { if (a == 1.5f && b == 2.5 && c && d == 3.5m) gets = gets + 1; return value; }
  set { if (a == 1.5f && b == 2.5 && c && d == 3.5m) sets = sets + 1; this.value = value; }
 }
}
class Program { public static int Main() { Grid g = new Grid(); FloatKey a; DoubleKey b; BoolKey c; DecimalKey d;
 g[a + a, b + b, !c, d + d] += 7;
 return FloatKey.hits == 1 && DoubleKey.hits == 1 && BoolKey.hits == 1 && DecimalKey.hits == 1 &&
        g.gets == 1 && g.sets == 1 && g.value == 7 ? 42 : 1; } }
''', encoding='utf-8')
        for level in ('O0', 'O2'):
            executable = root / (level + '.exe')
            command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level,
                       '-o', str(executable), '--no-artifact-cache']
            result = subprocess.run(command, cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
            if result.returncode:
                raise AssertionError(('scalar indices', level, result.stdout + result.stderr))
            result = subprocess.run([str(executable)], cwd=root, timeout=120)
            if result.returncode != 42:
                raise AssertionError(('scalar indices', level, result.returncode))
        dispatch_cases = {
            'struct-local': '''struct Key { public static int hits; public static int operator +(Key a, Key b) { hits = hits + 1; return 0; } }
struct Container { public int data; public int this[int i] { get { return data; } set { data = value; } } }
class Program { public static int Main() { Container c; Key k; c[k + k] += 7;
 return Key.hits == 1 && c.data == 7 ? 42 : 1; } }''',
            'struct-ref-parameter': '''struct Key { public static int hits; public static int operator +(Key a, Key b) { hits = hits + 1; return 0; } }
struct Container { public int data; public int this[int i] { get { return data; } set { data = value; } } }
class Program { static void Change(ref Container c) { Key k; c[k + k] += 7; }
 public static int Main() { Container c; Change(ref c); return Key.hits == 1 && c.data == 7 ? 42 : 1; } }''',
            'struct-this': '''struct Key { public static int hits; public static int operator +(Key a, Key b) { hits = hits + 1; return 0; } }
struct Container { public int data; public int this[int i] { get { return data; } set { data = value; } }
 public void Change() { Key k; this[k + k] += 7; } }
class Program { public static int Main() { Container c; c.Change(); return Key.hits == 1 && c.data == 7 ? 42 : 1; } }''',
            'virtual': '''struct Key { public static int hits; public static int operator +(Key a, Key b) { hits = hits + 1; return 0; } }
class Base { public int data; public virtual int this[int i] { get { return data; } set { data = value; } } }
class Derived : Base { public override int this[int i] { get { return data + 1; } set { data = value; } } }
class Program { public static int Main() { Base b = new Derived(); Key k; b[k + k] += 7;
 return Key.hits == 1 && b.data == 8 ? 42 : 1; } }''',
            'interface': '''struct Key { public static int hits; public static int operator +(Key a, Key b) { hits = hits + 1; return 0; } }
interface IContainer { int this[int i] { get; set; } }
class Container : IContainer { public int data; public int this[int i] { get { return data; } set { data = value; } } }
class Program { public static int Main() { IContainer value = new Container(); Key k;
 value[k + k] += 7; return Key.hits == 1 && value[0] == 7 ? 42 : 1; } }''',
        }
        for name, source in dispatch_cases.items():
            (root / 'Main.ns').write_text(source, encoding='utf-8')
            for level in ('O0', 'O2'):
                executable = root / (level + '.exe')
                command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level,
                           '-o', str(executable), '--no-artifact-cache']
                result = subprocess.run(command, cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
                if result.returncode:
                    raise AssertionError((name, level, result.stdout + result.stderr))
                result = subprocess.run([str(executable)], cwd=root, timeout=120)
                if result.returncode != 42:
                    raise AssertionError((name, level, result.returncode))
        owned_source = '''namespace NativeSharp.Core { public class Exception { } }
using NativeSharp.Core;
class Gate { static Gate() throws Exception { throw new Exception(); } public static int Read() { return 1; } }
class Container {
 public static int calls; public static int drops; public static int gets; public static int sets;
 public int data;
 ~Container() { drops = drops + 1; }
 public static owned Container operator +(Container value, int offset) { calls = calls + 1; return new Container(); }
 public int Number { get { gets = gets + 1; GETTER return data; } set { sets = sets + 1; SETTER data = value; } }
}
class Program {
 static void Work() throws Exception { Container c = new Container(); (c + 1).Number += RHS; }
 public static int Main() { try { Work(); } catch (Exception error) {
  return Container.calls == 1 && Container.drops == 2 && Container.gets == EXPECT_GETS && Container.sets == EXPECT_SETS ? 42 : 1;
 } return 2; }
}'''
        normal_source = (owned_source.replace('GETTER', '').replace('SETTER', '').replace('RHS', '7')
                         .replace('static void Work() throws Exception', 'static void Work()')
                         .replace('try { Work(); } catch (Exception error) {', 'Work(); {')
                         .replace('EXPECT_GETS', '1').replace('EXPECT_SETS', '1'))
        (root / 'Main.ns').write_text(normal_source, encoding='utf-8')
        for level in ('O0', 'O2'):
            executable = root / (level + '.exe')
            command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level,
                       '-o', str(executable), '--no-artifact-cache']
            result = subprocess.run(command, cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
            if result.returncode:
                raise AssertionError(('owned receiver', level, result.stdout + result.stderr))
            result = subprocess.run([str(executable)], cwd=root, timeout=120)
            if result.returncode != 42:
                raise AssertionError(('owned receiver', level, result.returncode))
        owned_index_source = (normal_source.replace('class Container {',
            'struct Key { public static int hits; public static int operator +(Key a, Key b) { hits = hits + 1; return 0; } }\nclass Container {')
            .replace('public int Number {', 'public int this[int i] {')
            .replace('Container c = new Container(); (c + 1).Number += 7;',
                     'Container c = new Container(); Key k; (c + 1)[k + k] += 7;')
            .replace('Container.calls == 1 &&', 'Key.hits == 1 && Container.calls == 1 &&'))
        (root / 'Main.ns').write_text(owned_index_source, encoding='utf-8')
        for level in ('O0', 'O2'):
            executable = root / (level + '.exe')
            command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level,
                       '-o', str(executable), '--no-artifact-cache']
            result = subprocess.run(command, cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
            if result.returncode:
                raise AssertionError(('owned indexer receiver', level, result.stdout + result.stderr))
            result = subprocess.run([str(executable)], cwd=root, timeout=120)
            if result.returncode != 42:
                raise AssertionError(('owned indexer receiver', level, result.returncode))
        for name, getter, setter, rhs, gets, sets in (
                ('getter-throws', 'int ignored = Gate.Read();', '', '7', '1', '0'),
                ('rhs-throws', '', '', 'Gate.Read()', '1', '0'),
                ('setter-throws', '', 'int ignored = Gate.Read();', '7', '1', '1')):
            source = (owned_source.replace('GETTER', getter).replace('SETTER', setter).replace('RHS', rhs)
                      .replace('EXPECT_GETS', gets).replace('EXPECT_SETS', sets))
            (root / 'Main.ns').write_text(source, encoding='utf-8')
            for level in ('O0', 'O2'):
                executable = root / (level + '.exe')
                command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level,
                           '-o', str(executable), '--no-artifact-cache']
                result = subprocess.run(command, cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
                if result.returncode:
                    raise AssertionError((name, level, result.stdout + result.stderr))
                result = subprocess.run([str(executable)], cwd=root, timeout=120)
                if result.returncode != 42:
                    raise AssertionError((name, level, result.returncode))
        owned_types = TYPES.replace(
            'public static Container operator +(Container value, int offset) { receiverHits = receiverHits + 1; return value; }',
            'public static owned Container operator +(Container value, int offset) { return new Container(); }')
        (root / 'Main.ns').write_text(owned_types + '''class Program {
 public static int Main() { Container c = new Container(); (c + 1).Number += 7; return 42; }
}''', encoding='utf-8')
        result = subprocess.run([str(compiler), '--check-project-method-bodies', str(project)], cwd=root,
                                capture_output=True, text=True, errors='replace', timeout=120)
        if result.returncode:
            raise AssertionError(('owned receiver', result.stdout + result.stderr))
        (root / 'Main.ns').write_text(TYPES + '''class Program {
 public static int Main() { Container c = new Container(); int i = 0;
  c[i + 1] += 7; return c.data == 7 ? 42 : 1; }
}''', encoding='utf-8')
        for level in ('O0', 'O2'):
            executable = root / (level + '.exe')
            command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level,
                       '-o', str(executable), '--no-artifact-cache']
            result = subprocess.run(command, cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
            if result.returncode:
                raise AssertionError((level, result.stdout + result.stderr))
            result = subprocess.run([str(executable)], cwd=root, timeout=120)
            if result.returncode != 42:
                raise AssertionError((level, result.returncode))
    print('operator_accessor_reevaluation: PASS')
    return 0


if __name__ == '__main__':
    sys.exit(main())
