"""Prevent a borrowed class from escaping a temporary in-operator operand."""
from pathlib import Path
import subprocess
import sys
import tempfile


TYPES = '''class Payload { public int Value; public Payload() { Value = 42; } }
struct Owner {
 public owned Payload item;
 public Owner(int value) { item = new Payload(); }
 public static Payload operator +(in Owner a, in Owner b) { return a.item; }
}
'''
TUPLE_TYPES = TYPES.replace('Payload operator +', '(Payload, int) operator +').replace(
    'return a.item;', 'return (a.item, 0);')
SCALAR_TUPLE_TYPES = TYPES.replace('Payload operator +', '(int, int) operator +').replace(
    'return a.item;', 'return (a.item.Value, 0);')
BORROWED_LOCAL_TYPES = TYPES.replace('Payload operator +', 'int operator +').replace(
    'return a.item;', 'borrowed Payload alias = a.item; return alias.Value;')
BORROWED_MUTATION_TYPES = BORROWED_LOCAL_TYPES.replace(
    'return alias.Value;', 'alias.Value = 9; return alias.Value;')
BORROWED_ESCAPE_TYPES = TYPES.replace(
    'return a.item;', 'borrowed Payload alias = a.item; return alias;')
BORROWED_GLOBAL_TYPES = TYPES.replace(
    'struct Owner {', 'class Store { public static Payload Value; }\nstruct Owner {').replace(
    'Payload operator +', 'int operator +').replace(
    'return a.item;', 'borrowed Payload alias = a.item; Store.Value = alias; return 42;')
BORROWED_RETURN_TYPES = BORROWED_ESCAPE_TYPES.replace('Payload operator +', 'borrowed Payload operator +')
GLOBAL_TYPES = TYPES.replace('struct Owner {', 'class Registry { public static Payload Global; }\nstruct Owner {').replace(
    'return a.item;', 'return Registry.Global;')
CASES = {
    'local-owner': ('''class Program { public static int Main() { Owner a = new Owner(1);
 Owner b = new Owner(2); Payload result = a + b; return result.Value; } }''', False),
    'two-temporaries': ('''class Program { public static Payload Leak() {
 return new Owner(1) + new Owner(2); } public static int Main() { return 42; } }''', False),
    'one-temporary': ('''class Program { public static Payload Leak(Owner local) {
 return local + new Owner(2); } public static int Main() { return 42; } }''', False),
    'tuple-escape': ('''class Program { public static (Payload, int) Leak() {
 return new Owner(1) + new Owner(2); } public static int Main() { return 42; } }''', False),
    'scalar-tuple': ('''class Program { public static int Main() {
 var (value, ignored) = new Owner(1) + new Owner(2); return value; } }''', True),
    'borrowed-local-use': ('''class Program { public static int Main() {
 Owner a = new Owner(1); Owner b = new Owner(2); return a + b; } }''', True),
    'borrowed-temporary-use': ('''class Program { public static int Main() {
 return new Owner(1) + new Owner(2); } }''', True),
    'borrowed-local-mutation': ('''class Program { public static int Main() { return 42; } }''', False),
    'borrowed-local-escape': ('''class Program { public static int Main() { return 42; } }''', False),
    'borrowed-local-global': ('''class Program { public static int Main() { return 42; } }''', False),
    'borrowed-temporary-escape': ('''class Program { public static borrowed Payload Leak() {
 return new Owner(1) + new Owner(2); } public static int Main() { return 42; } }''', False),
    'global-local-control': ('''class Program { public static int Main() {
 Registry.Global = new Payload(); Owner a = new Owner(1); Owner b = new Owner(2);
 Payload result = a + b; return result.Value; } }''', True),
    'global-two-temporaries': ('''class Program { public static Payload Leak() {
 return new Owner(1) + new Owner(2); } public static int Main() { return 42; } }''', False),
    'global-one-temporary': ('''class Program { public static Payload Leak(Owner local) {
 return local + new Owner(2); } public static int Main() { return 42; } }''', False),
}


def main():
    compiler = Path(sys.argv[1]).resolve()
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-in-operator-class-') as temporary:
        root = Path(temporary)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>OperatorClass</Name></PropertyGroup>'
                           '<ItemGroup><Compile Include="Main.ns"/></ItemGroup></Project>', encoding='utf-8')
        for name, (body, valid) in CASES.items():
            types = (GLOBAL_TYPES if name.startswith('global-') else TUPLE_TYPES if name == 'tuple-escape'
                     else SCALAR_TUPLE_TYPES if name == 'scalar-tuple'
                     else BORROWED_LOCAL_TYPES if name in ('borrowed-local-use', 'borrowed-temporary-use')
                     else BORROWED_MUTATION_TYPES if name == 'borrowed-local-mutation'
                     else BORROWED_ESCAPE_TYPES if name == 'borrowed-local-escape'
                     else BORROWED_RETURN_TYPES if name == 'borrowed-temporary-escape'
                     else BORROWED_GLOBAL_TYPES if name == 'borrowed-local-global' else TYPES)
            (root / 'Main.ns').write_text(types + body, encoding='utf-8')
            for level in ('O0', 'O2'):
                executable = root / (name + '-' + level + '.exe')
                command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level,
                           '-o', str(executable), '--no-artifact-cache']
                built = subprocess.run(command, cwd=root, capture_output=True, text=True,
                                       errors='replace', timeout=120)
                output = built.stdout + built.stderr
                print('COMMAND', subprocess.list2cmdline(command), 'EXIT', built.returncode, output, flush=True)
                if not valid:
                    expected = ('borrowed value cannot escape through an unqualified return type'
                                if name in ('local-owner', 'borrowed-local-escape') else
                                'left side of assignment is not assignable'
                                if name == 'borrowed-local-mutation' else
                                'borrowed value cannot be stored in field'
                                if name == 'borrowed-local-global' else 'no viable')
                    if built.returncode == 0 or expected not in output:
                        failures.append((name, level, 'expected rejection', built.returncode))
                    continue
                if built.returncode:
                    failures.append((name, level, 'build'))
                    continue
                result = subprocess.run([str(executable)], cwd=root, capture_output=True,
                                        text=True, errors='replace', timeout=120)
                print('EXECUTE', name, level, 'EXIT', result.returncode, flush=True)
                if result.returncode != 42:
                    failures.append((name, level, 'execute', result.returncode))
    if failures:
        print('FAIL', failures, file=sys.stderr, flush=True)
        return 1
    print('operator_in_temporary_class: PASS', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
