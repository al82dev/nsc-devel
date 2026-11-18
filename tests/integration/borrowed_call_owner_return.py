"""Check that a borrowed call result cannot escape a local owner."""
from pathlib import Path
import subprocess
import sys
import tempfile


PREFIX = '''class Box { public int Value; public Box(int value) { Value = value; } }
'''
CASES = {
    'method-local-escape': ('''class Program {
 static borrowed Box Identity(borrowed Box value) { return value; }
 static borrowed Box Escape() { raii Box owner = new Box(10); return Identity(owner); }
 public static int Main() { return 42; } }''', "borrowed return value outlives local owner 'owner'"),
    'namespace-local-escape': ('''borrowed Box Identity(borrowed Box value) { return value; }
borrowed Box Escape() { raii Box owner = new Box(10); return Identity(owner); }
int Main() { return 42; }''', "borrowed return value outlives local owner 'owner'"),
    'method-nested-local-escape': ('''class Program {
 static borrowed Box Identity(borrowed Box value) { return value; }
 static borrowed Box Escape() { raii Box owner = new Box(10); return Identity(Identity(owner)); }
 public static int Main() { return 42; } }''', "borrowed return value outlives local owner 'owner'"),
    'namespace-nested-local-escape': ('''borrowed Box Identity(borrowed Box value) { return value; }
borrowed Box Escape() { raii Box owner = new Box(10); return Identity(Identity(owner)); }
int Main() { return 42; }''', "borrowed return value outlives local owner 'owner'"),
    'method-local-alias-escape': ('''class Program {
 static borrowed Box Identity(borrowed Box value) { return value; }
 static borrowed Box Escape() { raii Box owner = new Box(10); borrowed Box view = owner; return Identity(view); }
 public static int Main() { return 42; } }''', "borrowed return value outlives local owner 'owner'"),
    'method-conditional-local-escape': ('''class Program {
 static borrowed Box Identity(borrowed Box value) { return value; }
 static borrowed Box Escape(bool choose, borrowed Box other) { raii Box owner = new Box(10);
 return Identity(choose ? owner : other); }
 public static int Main() { return 42; } }''', ('requires a checked owner or borrowed source', "borrowed return value outlives local owner 'owner'")),
    'namespace-conditional-local-escape': ('''borrowed Box Identity(borrowed Box value) { return value; }
borrowed Box Escape(bool choose, borrowed Box other) { raii Box owner = new Box(10);
 return Identity(choose ? owner : other); }
int Main() { return 42; }''', ('no viable overload', "borrowed return value outlives local owner 'owner'")),
    'method-owned-parameter-escape': ('''class Program {
 static borrowed Box Identity(borrowed Box value) { return value; }
 static borrowed Box Escape(owned Box owner) { return Identity(owner); }
 public static int Main() { return 42; } }''', "borrowed return value refers to owned parameter 'owner' whose lifetime ends when the method returns"),
    'namespace-owned-parameter-escape': ('''borrowed Box Identity(borrowed Box value) { return value; }
borrowed Box Escape(owned Box owner) { return Identity(owner); }
int Main() { return 42; }''', "borrowed return value refers to owned parameter 'owner' whose lifetime ends when the method returns"),
    'method-parameter-forward': ('''class Program {
 static borrowed Box Identity(borrowed Box value) { return value; }
 static borrowed Box Forward(borrowed Box value) { return Identity(value); }
 public static int Main() { return 42; } }''', None),
    'namespace-parameter-forward': ('''borrowed Box Identity(borrowed Box value) { return value; }
borrowed Box Forward(borrowed Box value) { return Identity(value); }
int Main() { return 42; }''', None),
}


def main():
    compiler = str(Path(sys.argv[1]).resolve())
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-borrowed-call-return-') as temporary:
        root = Path(temporary)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>BorrowedCallReturn</Name></PropertyGroup>'
                           '<ItemGroup><Compile Include="Main.ns" /></ItemGroup></Project>', encoding='utf-8')
        for name, (body, expected) in CASES.items():
            (root / 'Main.ns').write_text(PREFIX + body, encoding='utf-8')
            command = [compiler, '--check-project-method-bodies', str(project)]
            result = subprocess.run(command, cwd=root, capture_output=True, text=True,
                                    errors='replace', timeout=60)
            output = result.stdout + result.stderr
            print(name, subprocess.list2cmdline(command), 'EXIT', result.returncode, output, flush=True)
            if expected is None:
                if result.returncode != 0:
                    failures.append((name, 'expected acceptance'))
            elif result.returncode == 0 or not any(
                    message in output for message in (expected if isinstance(expected, tuple) else (expected,))):
                failures.append((name, 'expected local-owner diagnostic'))
    print('FAILURES', failures, flush=True)
    return bool(failures)


if __name__ == '__main__':
    sys.exit(main())
