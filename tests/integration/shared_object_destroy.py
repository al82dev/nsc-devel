import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def main():
    compiler, storage_source = map(Path, sys.argv[1:3])
    source = '''using NativeSharp.Core.Internal;
class Payload { public static int Drops; public int Value; ~Payload() { Drops = Drops + 1; } }
class Derived : Payload { public static int DerivedDrops; ~Derived() { DerivedDrops = DerivedDrops + 1; } }
class Large { public long A; public long B; public long C; public long D; }
class DerivedLarge : Large { public long E; }
class Factory { public static T Place<T>(T* storage) where T : class, new() { return new (storage) T(); } }
class Program { public static int Main() { unsafe("placement lifetime") {
 if (SharedStorage.InstanceSize<Large>(null) != 40l || SharedStorage.InstanceAlignment<Large>(null) != 8l) return 6;
 if (SharedStorage.InstanceSize<DerivedLarge>(null) != 48l) return 7;
 long* memory = stackalloc long[32]; Payload* slot = (Payload*)memory;
 Payload p = Factory.Place<Payload>(slot); p.Value = 42;
 if (p.Value != 42) return 1;
 SharedStorage.DestroyObject<Payload>(p);
 SharedStorage.DestroyObject<Payload>(null);
 if (Payload.Drops != 1) return 2;
 long* otherMemory = stackalloc long[32]; Derived* otherSlot = (Derived*)otherMemory;
 Derived derived = Factory.Place<Derived>(otherSlot); Payload baseValue = derived;
 SharedStorage.DestroyObject<Payload>(baseValue);
 if (Payload.Drops != 2 || Derived.DerivedDrops != 1) return 4;
 otherMemory[0] = 88l; if (otherMemory[0] != 88l) return 5;
 memory[0] = 99l; return memory[0] == 99l ? 42 : 3;
} } }
'''
    with tempfile.TemporaryDirectory(prefix='nsc-shared-destroy-') as tmp:
        root = Path(tmp)
        shutil.copyfile(storage_source, root / 'SharedStorage.ns')
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name><Language>3</Language></PropertyGroup>'
                           '<ItemGroup><Compile Include="*.ns" /></ItemGroup></Project>', encoding='utf-8')
        source_path = root / 'Main.ns'
        source_path.write_text(source, encoding='utf-8')
        result = subprocess.run([str(compiler), '--dump-project-oop-mir', str(project), '--no-banner'],
                                cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'object-destroy' in result.stdout and 'destruction=dynamic-object-only' in result.stdout
        if os.name == 'nt':
            for level in ('O0', 'O2'):
                exe = root / (level + '.exe')
                result = subprocess.run([str(compiler), '--emit-project-oop-exe', str(project), '-o', str(exe),
                                         '--no-artifact-cache', '-' + level], cwd=root, capture_output=True,
                                        text=True, errors='replace', timeout=120)
                assert result.returncode == 0, result.stdout + result.stderr
                assert subprocess.run([str(exe)], cwd=root, capture_output=True, timeout=60).returncode == 42
        source_path.write_text(source.replace('SharedStorage.DestroyObject<Payload>(p);',
                                             'SharedStorage.DestroyObject<string>("text");'), encoding='utf-8')
        result = subprocess.run([str(compiler), '--check-project-oop-hir', str(project)], cwd=root,
                                capture_output=True, text=True, errors='replace', timeout=120)
        assert result.returncode != 0 and 'requires a class with a NativeSharp object descriptor' in result.stdout + result.stderr
        source_path.write_text(source.replace('SharedStorage.InstanceSize<Large>(null)',
                                             'SharedStorage.InstanceSize<Large>(new Large())'), encoding='utf-8')
        result = subprocess.run([str(compiler), '--check-project-method-bodies', str(project)], cwd=root,
                                capture_output=True, text=True, errors='replace', timeout=120)
        assert result.returncode != 0 and 'requires a class type and a null witness' in result.stdout + result.stderr
        source_path.write_text('''class Resource { public static int Drops; ~Resource() { Drops = Drops + 1; } }
struct Value { public static int Drops; public owned Resource item; public long Number;
 ~Value() { Drops = Drops + 1; } }
class Program { public static int Main() { unsafe("placement lifetime") {
 long* words = stackalloc long[16]; Value* slot = (Value*)words; Value* empty = null;
 Value* first = new (slot) Value(); (*slot).item = new Resource(); (*slot).Number = 42l;
 if ((*slot).Number != 42l) return 1; destroy empty; destroy slot;
 if (Value.Drops != 1 || Resource.Drops != 1) return 2;
 Value* second = new (slot) Value(); (*slot).item = new Resource();
 destroy slot; if (Value.Drops != 2 || Resource.Drops != 2) return 3;
 words[0] = 99l; return words[0] == 99l ? 42 : 4;
} } }
''', encoding='utf-8')
        if os.name == 'nt':
            for level in ('O0', 'O2'):
                exe = root / ('value-' + level + '.exe')
                result = subprocess.run([str(compiler), '--emit-project-oop-exe', str(project), '-o', str(exe),
                                         '--no-artifact-cache', '-' + level], cwd=root, capture_output=True,
                                        text=True, errors='replace', timeout=120)
                assert result.returncode == 0, result.stdout + result.stderr
                result = subprocess.run([str(exe)], cwd=root, capture_output=True, timeout=60)
                assert result.returncode == 42, (level, result.returncode)
        source_path.write_text(source.replace('SharedStorage.InstanceSize<Large>(null)',
                                             'SharedStorage.InstanceSize<string>(null)'), encoding='utf-8')
        result = subprocess.run([str(compiler), '--check-project-method-bodies', str(project)], cwd=root,
                                capture_output=True, text=True, errors='replace', timeout=120)
        assert result.returncode != 0 and 'requires a class type and a null witness' in result.stdout + result.stderr
    return 0


if __name__ == '__main__':
    sys.exit(main())
