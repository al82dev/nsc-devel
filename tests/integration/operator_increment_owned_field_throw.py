"""Check owned field update cleanup when a user operator throws."""
from html import escape
from pathlib import Path
import subprocess
import sys
import tempfile


SOURCE = '''using NativeSharp.Core;
class Box { public static int Drops; public int Value;
 public Box(int value) { Value = value; } ~Box() { Drops = Drops + 1; }
 public static owned Box operator ++(Box value) throws Exception { throw new Exception(); }
 public static owned Box operator --(Box value) throws Exception { throw new Exception(); } }
class Holder { public owned Box Item; public Holder() { Item = new Box(10); } }
class Program { public static int Main() {
 int caught = 0;
 { raii Holder holder = new Holder();
   try { OPERATION; } catch (Exception error) { caught = caught + 1; }
   if (caught != 1 || holder.Item.Value != 10 || Box.Drops != 0) return 1; }
 return Box.Drops == 1 ? 42 : 2; } }
'''


def main():
    compiler, library, metadata = (Path(value).resolve() for value in sys.argv[1:4])
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-field-update-throw-') as temporary:
        root = Path(temporary)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>FieldUpdateThrow</Name></PropertyGroup>'
            '<ItemGroup><Compile Include="Main.ns"/><LibraryReference Include="' +
            escape(library.as_posix(), quote=True) + '" Metadata="' +
            escape(metadata.as_posix(), quote=True) + '"/></ItemGroup></Project>', encoding='utf-8')
        direct = (('prefix-plus', '++holder.Item'), ('prefix-minus', '--holder.Item'),
                  ('postfix-plus', 'holder.Item++'), ('postfix-minus', 'holder.Item--'))
        computed = tuple(('computed-' + name, operation.replace('holder.Item', 'Select(holder).Item'))
                         for name, operation in direct)
        for name, operation in direct + computed:
            source = SOURCE.replace('OPERATION', operation)
            if name.startswith('computed-'):
                source = source.replace('class Program {', '''class Program {
 static int Calls;
 static borrowed Holder Select(borrowed Holder value) { Calls = Calls + 1; return value; }
''').replace('caught != 1 ||', 'caught != 1 || Calls != 1 ||')
            (root / 'Main.ns').write_text(source, encoding='utf-8')
            for level in ('O0', 'O2'):
                executable = root / (name + '-' + level + '.exe')
                command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level,
                           '-o', str(executable), '--no-artifact-cache']
                result = subprocess.run(command, cwd=root, capture_output=True, text=True,
                                        errors='replace', timeout=120)
                print('COMMAND', subprocess.list2cmdline(command), 'EXIT', result.returncode,
                      result.stdout + result.stderr, flush=True)
                if result.returncode:
                    failures.append((name, level, 'compile'))
                    continue
                result = subprocess.run([str(executable)], cwd=root, capture_output=True, text=True,
                                        errors='replace', timeout=120)
                print('EXECUTE', name, level, 'EXIT', result.returncode, result.stdout + result.stderr, flush=True)
                if result.returncode != 42:
                    failures.append((name, level, 'execute', result.returncode))
    print('FAILURES', failures, flush=True)
    return bool(failures)


if __name__ == '__main__':
    sys.exit(main())
