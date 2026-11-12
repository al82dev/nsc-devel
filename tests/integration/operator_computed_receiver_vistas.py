"""Check one evaluation and field ownership for computed ++/-- receivers."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile


PREFIX = '''class Box { public static int Drops; public int Value;
 public Box(int value) { Value = value; } ~Box() { Drops = Drops + 1; }
 public static owned Box operator ++(Box value) { return new Box(value.Value + 1); }
 public static owned Box operator --(Box value) { return new Box(value.Value - 1); } }
class Holder { public owned Box Item; public Holder() { Item = new Box(10); } }
class Program { static int Calls;
 static borrowed Holder Select(borrowed Holder value) { Calls = Calls + 1; return value; }
 static borrowed Box Identity(borrowed Box value) { return value; }
'''

VALID = {
    'prefix-increment': '''public static int Main() {
 { raii Holder holder = new Holder(); { borrowed Box view = ++Select(holder).Item;
   if (view.Value != 11) return 1; }
   if (holder.Item.Value != 11 || Calls != 1 || Box.Drops != 1) return 1; }
 return Box.Drops == 2 ? 42 : 2; }''',
    'prefix-decrement': '''public static int Main() {
 { raii Holder holder = new Holder(); { borrowed Box view = --Select(holder).Item;
   if (view.Value != 9) return 1; }
   if (holder.Item.Value != 9 || Calls != 1 || Box.Drops != 1) return 1; }
 return Box.Drops == 2 ? 42 : 2; }''',
    'postfix-increment': '''public static int Main() {
 { raii Holder holder = new Holder(); { raii Box old = Select(holder).Item++;
   if (old.Value != 10 || holder.Item.Value != 11 || Calls != 1 || Box.Drops != 0) return 1; }
   if (Box.Drops != 1) return 2; } return Box.Drops == 2 ? 42 : 3; }''',
    'postfix-decrement': '''public static int Main() {
 { raii Holder holder = new Holder(); { raii Box old = Select(holder).Item--;
   if (old.Value != 10 || holder.Item.Value != 9 || Calls != 1 || Box.Drops != 0) return 1; }
   if (Box.Drops != 1) return 2; } return Box.Drops == 2 ? 42 : 3; }''',
}

INVALID = {
    'live-view-before-update': ('''public static int Main() { raii Holder holder = new Holder();
 borrowed Box view = Identity(holder.Item); ++Select(holder).Item; return view.Value; }''',
        ('cannot overwrite owner field while a borrowed view is alive',
         'cannot pass object with a borrowed field view to a potentially mutating call')),
    'replacement-after-update': ('''public static int Main() { raii Holder holder = new Holder();
 borrowed Box view = ++Select(holder).Item; holder.Item = new Box(30); return view.Value; }''',
        ('cannot overwrite owner field while a borrowed view is alive',)),
    'escape-local-owner': ('''static borrowed Box Escape() { raii Holder holder = new Holder();
 return ++Select(holder).Item; } public static int Main() { return 42; }''',
        ("borrowed return value outlives local owner 'holder'",)),
}


def run(command, repository):
    """Run one checked compiler invocation or one previously accepted executable."""
    command = [str(part) for part in command]
    result = subprocess.run(command, cwd=repository, capture_output=True, text=True,
                            errors='replace', timeout=120)
    print('COMMAND', subprocess.list2cmdline(command), 'EXIT', result.returncode,
          result.stdout + result.stderr, flush=True)
    return result


def main():
    compiler = Path(sys.argv[1]).resolve()
    if compiler.drive.casefold() != 'a:':
        raise RuntimeError('compiler must be on A:')
    repository = Path(__file__).resolve().parents[2]
    for variable in ('TEMP', 'TMP'):
        location = os.environ.get(variable)
        if not location or Path(location).resolve().drive.casefold() != 'a:':
            raise RuntimeError(variable + ' must identify a directory on A:')
    if Path(tempfile.gettempdir()).resolve().drive.casefold() != 'a:':
        raise RuntimeError('Python temporary directory must be on A:')
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-vistas-computed-operator-') as temporary:
        root = Path(temporary)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup>'
                           '<ItemGroup><Compile Include="Main.ns"/></ItemGroup></Project>', encoding='utf-8')
        for name, body in VALID.items():
            (root / 'Main.ns').write_text(PREFIX + body + ' }', encoding='utf-8')
            for level in ('O0', 'O2'):
                executable = root / (name + '-' + level + '.exe')
                built = run([compiler, '--emit-project-oop-exe', project, '-' + level,
                             '-o', executable, '--no-artifact-cache', '--no-error-log'], repository)
                if built.returncode:
                    failures.append((name, level, 'compile', built.returncode))
                    continue
                executed = run([executable], repository)
                if executed.returncode != 42:
                    failures.append((name, level, 'execute', executed.returncode))
        for name, (body, expected) in INVALID.items():
            (root / 'Main.ns').write_text(PREFIX + body + ' }', encoding='utf-8')
            checked = run([compiler, '--check-project-method-bodies', project, '--no-error-log'], repository)
            output = checked.stdout + checked.stderr
            unsupported = 'user-defined increment/decrement update is not implemented'
            if checked.returncode == 0 or unsupported in output or not any(text in output for text in expected):
                failures.append((name, 'missing ownership diagnostic', checked.returncode))
    print('FAILURES', failures, flush=True)
    return bool(failures)


if __name__ == '__main__':
    sys.exit(main())
