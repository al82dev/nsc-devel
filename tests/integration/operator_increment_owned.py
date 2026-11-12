"""Check ownership and destruction for class increment/decrement operators."""
from pathlib import Path
import subprocess
import sys
import tempfile


SOURCE = '''class Box {
 public static int Drops;
 public int Value;
 public Box(int value) { Value = value; }
 ~Box() { Drops = Drops + 1; }
 public int Add(borrowed Box other) { return Value + other.Value; }
 public static owned Box operator ++(Box value) { return new Box(value.Value + 1); }
 public static owned Box operator --(Box value) { return new Box(value.Value - 1); }
}
class Program {
 static int Read(borrowed Box item) { return item.Value; }
 static int Sum(borrowed Box first, borrowed Box second) { return first.Value + second.Value; }
 static borrowed Box Identity(borrowed Box value) { return value; }
 static int Consume(owned Box item) { return item.Value; }
 static owned Box Transfer() { raii Box value = new Box(40); return value++; }
 public static int Main() {
  {
   raii Box value = new Box(10);
   {
    raii Box old = value++;
    if (old.Value != 10 || value.Value != 11 || Box.Drops != 0) return 1;
    {
     borrowed Box updated = --value;
     if (updated.Value != 10 || value.Value != 10 || Box.Drops != 1) return 2;
    }
    if (old.Value != 10 || Box.Drops != 1) return 3;
   }
   if (value.Value != 10 || Box.Drops != 2) return 4;
  }
  if (Box.Drops != 3) return 5;
  {
   raii Box discarded = new Box(2);
   discarded++;
   if (discarded.Value != 3 || Box.Drops != 4) return 6;
  }
  if (Box.Drops != 5) return 7;
  {
   raii Box value = new Box(20);
   int oldValue = Read(value++);
   if (oldValue != 20 || value.Value != 21 || Box.Drops != 6) return 8;
  }
  if (Box.Drops != 7) return 9;
  {
   raii Box value = new Box(30);
   int oldValue = (value++).Value;
   if (oldValue != 30 || value.Value != 31 || Box.Drops != 8) return 10;
  }
  if (Box.Drops != 9) return 11;
  {
   raii Box old = Transfer();
   if (old.Value != 40 || Box.Drops != 10) return 12;
  }
  if (Box.Drops != 11) return 13;
  {
   raii Box value = new Box(50);
   int oldValue = Consume(value++);
   if (oldValue != 50 || value.Value != 51 || Box.Drops != 12) return 14;
  }
  if (Box.Drops != 13) return 15;
  {
   raii Box first = new Box(1);
   raii Box second = new Box(2);
   if (Sum(++first, ++second) != 5 || Box.Drops != 15) return 16;
   ++first;
   if (first.Value != 3 || Box.Drops != 16) return 17;
   if (Sum(first, first) != 6 || Box.Drops != 16) return 21;
  }
  if (Box.Drops != 18) return 18;
  {
   raii Box first = new Box(3);
   raii Box second = new Box(4);
   if (Sum(Identity(++first), ++second) != 9 || Box.Drops != 20) return 19;
  }
  if (Box.Drops != 22) return 20;
  {
   raii Box first = new Box(5);
   raii Box second = new Box(6);
   if ((++first).Add(++second) != 13 || Box.Drops != 24) return 22;
   if (first.Add(first) != 12) return 23;
  }
  return Box.Drops == 26 ? 42 : 24;
 }
}
'''

ACTIVE_BORROW = '''class Box {
 public int Value;
 public Box(int value) { Value = value; }
 public static owned Box operator ++(Box value) { return new Box(value.Value + 1); }
}
class Program {
 public static int Main() {
  raii Box value = new Box(10);
  borrowed Box view = value;
  value++;
  return view.Value;
 }
}
'''

PINNED_OWNER = '''class Box {
 public int Value;
 public Box(int value) { Value = value; }
 public static owned Box operator ++(Box value) { return new Box(value.Value + 1); }
}
class Program {
 public static int Main() {
  pin raii Box stable = new Box(10);
  stable++;
  return 42;
 }
}
'''

PREFIX_ESCAPE = '''class Box {
 public int Value;
 public Box(int value) { Value = value; }
 public static owned Box operator ++(Box value) { return new Box(value.Value + 1); }
}
class Program {
 static Box Escape() { raii Box value = new Box(10); return ++value; }
 public static int Main() { return 42; }
}
'''

PREFIX_OWNER_COPY = '''class Box {
 public int Value;
 public Box(int value) { Value = value; }
 public static owned Box operator ++(Box value) { return new Box(value.Value + 1); }
}
class Program {
 public static int Main() {
  raii Box value = new Box(10);
  raii Box other = ++value;
  return 42;
 }
}
'''

PREFIX_BORROWED_ESCAPE = '''class Box {
 public int Value;
 public Box(int value) { Value = value; }
 public static owned Box operator ++(Box value) { return new Box(value.Value + 1); }
}
class Program {
 static borrowed Box Escape() { raii Box value = new Box(10); return ++value; }
 public static int Main() { return 42; }
}
'''

PREFIX_FIELD_ESCAPE = '''class Box {
 public int Value;
 public Box(int value) { Value = value; }
 public static owned Box operator ++(Box value) { return new Box(value.Value + 1); }
}
class Holder { public Box Item; }
class Program {
 public static int Main() {
  raii Box value = new Box(10);
  raii Holder holder = new Holder();
  holder.Item = ++value;
  return 42;
 }
}
'''

PREFIX_INVALIDATED_ARGUMENT = '''class Box {
 public int Value;
 public Box(int value) { Value = value; }
 public static owned Box operator ++(Box value) { return new Box(value.Value + 1); }
}
class Program {
 static int Both(borrowed Box first, borrowed Box second) { return first.Value + second.Value; }
 public static int Main() {
  raii Box value = new Box(10);
  return Both(++value, ++value);
 }
}
'''

PREFIX_INVALIDATED_NAMESPACE = '''class Box {
 public int Value;
 public Box(int value) { Value = value; }
 public static owned Box operator ++(Box value) { return new Box(value.Value + 1); }
}
int Both(borrowed Box first, borrowed Box second) { return first.Value + second.Value; }
int Main() { raii Box value = new Box(10); return Both(++value, ++value); }
'''

PREFIX_INVALIDATED_NESTED = '''class Box {
 public int Value;
 public Box(int value) { Value = value; }
 public static owned Box operator ++(Box value) { return new Box(value.Value + 1); }
}
class Program {
 static int Read(borrowed Box value) { return value.Value; }
 static int Both(borrowed Box first, int second) { return first.Value + second; }
 public static int Main() { raii Box value = new Box(10); return Both(++value, Read(++value)); }
}
'''

PREFIX_MOVED_ARGUMENT = '''class Box {
 public int Value;
 public Box(int value) { Value = value; }
 public static owned Box operator ++(Box value) { return new Box(value.Value + 1); }
}
class Program {
 static int Both(borrowed Box first, owned Box second) { return first.Value + second.Value; }
 public static int Main() { raii Box value = new Box(10); return Both(++value, move(value)); }
}
'''

OWNER_THEN_PREFIX = '''class Box {
 public int Value;
 public Box(int value) { Value = value; }
 public static owned Box operator ++(Box value) { return new Box(value.Value + 1); }
}
class Program {
 static int Both(borrowed Box first, borrowed Box second) { return first.Value + second.Value; }
 public static int Main() { raii Box value = new Box(10); return Both(value, ++value); }
}
'''

PREFIX_FORWARDED_ARGUMENT = '''class Box {
 public int Value;
 public Box(int value) { Value = value; }
 public static owned Box operator ++(Box value) { return new Box(value.Value + 1); }
}
class Program {
 static borrowed Box Identity(borrowed Box value) { return value; }
 static int Both(borrowed Box first, borrowed Box second) { return first.Value + second.Value; }
 public static int Main() { raii Box value = new Box(10); return Both(Identity(++value), ++value); }
}
'''

PREFIX_FORWARDED_NAMESPACE = '''class Box {
 public int Value;
 public Box(int value) { Value = value; }
 public static owned Box operator ++(Box value) { return new Box(value.Value + 1); }
}
borrowed Box Identity(borrowed Box value) { return value; }
int Both(borrowed Box first, borrowed Box second) { return first.Value + second.Value; }
int Main() { raii Box value = new Box(10); return Both(Identity(++value), ++value); }
'''

PREFIX_RECEIVER_INVALIDATED = '''class Box {
 public int Value;
 public Box(int value) { Value = value; }
 public int Add(borrowed Box other) { return Value + other.Value; }
 public static owned Box operator ++(Box value) { return new Box(value.Value + 1); }
}
class Program {
 public static int Main() { raii Box value = new Box(10); return (++value).Add(++value); }
}
'''

DIRECT_RECEIVER_INVALIDATED = '''class Box {
 public int Value;
 public Box(int value) { Value = value; }
 public int Add(borrowed Box other) { return Value + other.Value; }
 public static owned Box operator ++(Box value) { return new Box(value.Value + 1); }
}
class Program {
 public static int Main() { raii Box value = new Box(10); return value.Add(++value); }
}
'''

EXTENSION_RECEIVER_INVALIDATED = '''class Box {
 public int Value;
 public Box(int value) { Value = value; }
 public static owned Box operator ++(Box value) { return new Box(value.Value + 1); }
}
extension Box { public int Add(borrowed Box other) { return this.Value + other.Value; } }
class Program {
 public static int Main() { raii Box value = new Box(10); return (++value).Add(++value); }
}
'''

NAMESPACE_BORROW = '''class Box {
 public static int Drops;
 public int Value;
 public Box(int value) { Value = value; }
 ~Box() { Drops = Drops + 1; }
 public static owned Box operator ++(Box value) { return new Box(value.Value + 1); }
}
int Read(borrowed Box value) { return value.Value; }
int Main() {
 {
  raii Box value = new Box(10);
  int oldValue = Read(value++);
  if (oldValue != 10 || value.Value != 11 || Box.Drops != 1) return 1;
 }
 return Box.Drops == 2 ? 42 : 2;
}
'''

NAMESPACE_UNQUALIFIED = '''class Box {
 public int Value;
 public Box(int value) { Value = value; }
 public static owned Box operator ++(Box value) { return new Box(value.Value + 1); }
}
int Read(Box value) { return value.Value; }
int Main() { raii Box value = new Box(10); return Read(value++); }
'''

NAMESPACE_MOVE_BORROW = '''class Box { public Box() { } }
int Read(borrowed Box value) { return 1; }
int Main() { raii Box value = new Box(); return Read(move(value)); }
'''


def main():
    compiler = Path(sys.argv[1]).resolve()
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-owned-increment-') as temporary:
        root = Path(temporary)
        (root / 'Main.ns').write_text(SOURCE, encoding='utf-8')
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>OwnedIncrement</Name></PropertyGroup>'
                           '<ItemGroup><Compile Include="Main.ns"/></ItemGroup></Project>', encoding='utf-8')
        for level in ('O0', 'O2'):
            executable = root / (level + '.exe')
            command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level,
                       '-o', str(executable), '--no-artifact-cache']
            result = subprocess.run(command, cwd=root, capture_output=True, text=True,
                                    errors='replace', timeout=120)
            print('COMMAND', subprocess.list2cmdline(command), 'EXIT', result.returncode,
                  result.stdout + result.stderr, flush=True)
            if result.returncode:
                failures.append((level, 'compile'))
                continue
            result = subprocess.run([str(executable)], cwd=root, capture_output=True, text=True,
                                    errors='replace', timeout=120)
            print('EXECUTE', level, 'EXIT', result.returncode, result.stdout + result.stderr, flush=True)
            if result.returncode != 42:
                failures.append((level, 'execute', result.returncode))
        (root / 'Main.ns').write_text(NAMESPACE_BORROW, encoding='utf-8')
        for level in ('O0', 'O2'):
            executable = root / ('namespace-' + level + '.exe')
            command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level,
                       '-o', str(executable), '--no-artifact-cache']
            result = subprocess.run(command, cwd=root, capture_output=True, text=True,
                                    errors='replace', timeout=120)
            print('COMMAND', subprocess.list2cmdline(command), 'EXIT', result.returncode,
                  result.stdout + result.stderr, flush=True)
            if result.returncode:
                failures.append(('namespace', level, 'compile'))
                continue
            result = subprocess.run([str(executable)], cwd=root, capture_output=True, text=True,
                                    errors='replace', timeout=120)
            print('EXECUTE namespace', level, 'EXIT', result.returncode, result.stdout + result.stderr, flush=True)
            if result.returncode != 42:
                failures.append(('namespace', level, 'execute', result.returncode))
        (root / 'Main.ns').write_text(ACTIVE_BORROW, encoding='utf-8')
        command = [str(compiler), '--check-project-method-bodies', str(project)]
        result = subprocess.run(command, cwd=root, capture_output=True, text=True,
                                errors='replace', timeout=120)
        output = result.stdout + result.stderr
        print('COMMAND', subprocess.list2cmdline(command), 'EXIT', result.returncode, output, flush=True)
        if result.returncode == 0 or 'cannot overwrite owner' not in output:
            failures.append(('active-borrow', 'unexpected diagnostic'))
        (root / 'Main.ns').write_text(PINNED_OWNER, encoding='utf-8')
        result = subprocess.run(command, cwd=root, capture_output=True, text=True,
                                errors='replace', timeout=120)
        output = result.stdout + result.stderr
        print('COMMAND', subprocess.list2cmdline(command), 'EXIT', result.returncode, output, flush=True)
        if result.returncode == 0 or 'cannot replace pinned owner' not in output:
            failures.append(('pinned-owner', 'unexpected diagnostic'))
        overwrite = "cannot overwrite owner 'value' while a borrowed local is alive"
        no_overload = "no viable overload of namespace function 'Read' accepts the argument types"
        for name, source, diagnostic in (
                ('prefix-escape', PREFIX_ESCAPE, 'borrowed value cannot escape through an unqualified return type'),
                ('prefix-owner-copy', PREFIX_OWNER_COPY, 'RAII ownership cannot be copied or borrowed'),
                ('prefix-borrowed-escape', PREFIX_BORROWED_ESCAPE, "borrowed return value outlives local owner 'value'"),
                ('prefix-field-escape', PREFIX_FIELD_ESCAPE, "borrowed value cannot be stored in field 'Item'"),
                ('prefix-invalidated-argument', PREFIX_INVALIDATED_ARGUMENT, overwrite),
                ('prefix-invalidated-namespace', PREFIX_INVALIDATED_NAMESPACE, overwrite),
                ('prefix-invalidated-nested', PREFIX_INVALIDATED_NESTED, overwrite),
                ('prefix-moved-argument', PREFIX_MOVED_ARGUMENT, "cannot move owner 'value' while a borrowed local is alive"),
                ('owner-then-prefix', OWNER_THEN_PREFIX, overwrite),
                ('prefix-forwarded-argument', PREFIX_FORWARDED_ARGUMENT, overwrite),
                ('prefix-forwarded-namespace', PREFIX_FORWARDED_NAMESPACE, overwrite),
                ('prefix-receiver-invalidated', PREFIX_RECEIVER_INVALIDATED, overwrite),
                ('direct-receiver-invalidated', DIRECT_RECEIVER_INVALIDATED, overwrite),
                ('extension-receiver-invalidated', EXTENSION_RECEIVER_INVALIDATED, overwrite),
                ('namespace-unqualified', NAMESPACE_UNQUALIFIED, no_overload),
                ('namespace-move-borrow', NAMESPACE_MOVE_BORROW, no_overload)):
            (root / 'Main.ns').write_text(source, encoding='utf-8')
            result = subprocess.run(command, cwd=root, capture_output=True, text=True,
                                    errors='replace', timeout=120)
            output = result.stdout + result.stderr
            print('CHECK', name, 'COMMAND', subprocess.list2cmdline(command),
                  'EXIT', result.returncode, output, flush=True)
            if result.returncode == 0 or diagnostic not in output:
                failures.append((name, 'unexpected diagnostic'))
    if failures:
        print('FAIL', failures, file=sys.stderr, flush=True)
        return 1
    print('operator_increment_owned: PASS', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
