from pathlib import Path
import os
import subprocess
import sys
import tempfile


def run(command, cwd):
    return subprocess.run([str(part) for part in command], cwd=cwd, capture_output=True,
                          text=True, errors='replace', timeout=120)


def main():
    compiler = Path(sys.argv[1])
    with tempfile.TemporaryDirectory(prefix='nsc-shared-default-identity-') as temporary:
        root = Path(temporary)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name><Language>3</Language>'
                           '</PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
                           '</ItemGroup></Project>', encoding='utf-8')
        for family, internal in (('Rc', 'SharedLocalHandle'), ('WeakRc', 'SharedLocalWeak'),
                                 ('Arc', 'SharedAtomicHandle'), ('WeakArc', 'SharedAtomicWeak')):
            direct = '''namespace NativeSharp.Core {
 public class Exception { }
 public struct FAMILY<T> { public int value; public FAMILY(int v) { value = v; }
  ~FAMILY() { Counter.Drops = Counter.Drops + 1; } }
}
class Counter { public static int Drops; }
class Owner { public NativeSharp.Core.FAMILY<int> field;
 public Owner() throws NativeSharp.Core.Exception { throw new NativeSharp.Core.Exception(); } }
class Program { public static int Main() {
 NativeSharp.Core.FAMILY<int> seed = new NativeSharp.Core.FAMILY<int>(1);
 try { Owner value = new Owner(); } catch (NativeSharp.Core.Exception error) { }
 return Counter.Drops == 0 ? 42 : 1;
} }
'''
            nested = '''namespace NativeSharp.Core.Internal {
 public struct INNER<T> { public long[] block; public T* slot;
  ~INNER() { Counter.Drops = Counter.Drops + 1; } }
}
namespace NativeSharp.Core {
 public class Exception { }
 public struct FAMILY<T> { private NativeSharp.Core.Internal.INNER<T> inner;
  public int Stamp() { return 1; } }
}
class Counter { public static int Drops; }
class Owner { public NativeSharp.Core.FAMILY<int> field;
 public Owner() throws NativeSharp.Core.Exception { throw new NativeSharp.Core.Exception(); } }
class Program { public static int Main() {
 NativeSharp.Core.FAMILY<int> seed = default(NativeSharp.Core.FAMILY<int>);
 if (seed.Stamp() != 1) return 2;
 try { Owner value = new Owner(); } catch (NativeSharp.Core.Exception error) { }
 return Counter.Drops == 0 ? 42 : 1;
} }
'''
            for variant, source in (('direct', direct), ('nested', nested)):
                if os.environ.get('NSC_FAKE_SHARED_VARIANT') not in (None, variant):
                    continue
                (root / 'Main.ns').write_text(source.replace('FAMILY', family).replace('INNER', internal),
                                               encoding='utf-8')
                for level in ('O0', 'O2'):
                    exe = root / (family + '-' + variant + '-' + level + '.exe')
                    result = run([compiler, '--emit-project-oop-exe', project, '-' + level,
                                  '-o', exe, '--no-artifact-cache'], root)
                    assert result.returncode == 0, (family, variant, level, result.stdout + result.stderr)
                    result = run([exe], root)
                    print(family, variant, level, 'exit=' + str(result.returncode), flush=True)
                    assert result.returncode == 42, (family, variant, level, result.returncode)
    return 0


if __name__ == '__main__':
    sys.exit(main())
