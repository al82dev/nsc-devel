import os
from pathlib import Path
import subprocess
import sys
import tempfile


def run(arguments, cwd):
    return subprocess.run([str(value) for value in arguments], cwd=cwd,
                          capture_output=True, text=True, errors='replace', timeout=120)


def main():
    compiler = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix='nsc-internal-generic-') as temporary:
        root = Path(temporary)
        (root / 'Producer.nsproj').write_text(
            '<Project><PropertyGroup><Name>InternalGenericProducer</Name>'
            '<OutputType>StaticLibrary</OutputType></PropertyGroup><ItemGroup>'
            '<Compile Include="Producer.ns" /></ItemGroup></Project>', encoding='utf-8')
        (root / 'Producer.ns').write_text('''namespace InternalGeneric {
    public class Node {
        private int value;
        private Node next;
        internal void Set(Node item, int x) { next = item; value = x; }
        public int Get() { return value; }
    }
    public class Holder<T> where T : Node {
        private Node first;
        public void Assign(T node) { node . Set(first, 42); first = node; }
        ~Holder() { first = null; }
    }
}''', encoding='utf-8')
        library = root / 'Producer.lib'
        result = run([compiler, '--emit-project-oop-lib', root / 'Producer.nsproj',
                      '-O0', '-o', library], root)
        assert result.returncode == 0, result.stdout + result.stderr
        (root / 'Consumer.nsproj').write_text(
            '<Project><PropertyGroup><Name>Consumer</Name><OutputType>Exe</OutputType>'
            '</PropertyGroup><ItemGroup><Compile Include="Consumer.ns" />'
            '<LibraryReference Include="Producer.lib" Metadata="Producer.nsmeta" />'
            '</ItemGroup></Project>', encoding='utf-8')
        source = root / 'Consumer.ns'
        source.write_text('''using InternalGeneric;
public class Derived : Node { }
public class Program {
    public static int Main() {
        raii Derived node = new Derived();
        raii Derived other = new Derived();
        {
            raii Holder<Derived> holder = new Holder<Derived>();
            holder.Assign(node);
            holder.Assign(other);
        }
        return node.Get();
    }
}''', encoding='utf-8')
        executable = root / 'Consumer.exe'
        result = run([compiler, '--emit-project-oop-exe', root / 'Consumer.nsproj',
                      '-O0', '-o', executable], root)
        assert result.returncode == 0, result.stdout + result.stderr
        if os.name == 'nt':
            result = run([executable], root)
            assert result.returncode == 42, (result.returncode, result.stdout, result.stderr)
        source.write_text('''using InternalGeneric;
public class Program { public static int Main() {
    raii Node node = new Node(); node.Set(null, 42); return node.Get();
} }''', encoding='utf-8')
        result = run([compiler, '--check-project-method-bodies', root / 'Consumer.nsproj', '-O0'], root)
        assert result.returncode != 0 and "member 'Set' is not accessible" in (result.stdout + result.stderr), (
            result.returncode, result.stdout + result.stderr)
        print('PASS: binary generic internal dependency and access control')
    return 0


if __name__ == '__main__':
    sys.exit(main())
