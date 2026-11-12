"""Check owned payload fields when a shared payload destructor exits normally or throws."""
from pathlib import Path
from xml.sax.saxutils import escape
import hashlib
import os
import subprocess
import sys
import tempfile


def main():
    repo = Path(__file__).resolve().parents[2]
    compiler = Path(sys.argv[1]).resolve()
    configured = os.environ.get('NSC_TEST_CORE_BINARY')
    if not configured:
        print('SKIP: NSC_TEST_CORE_BINARY is not configured')
        return 77
    core = Path(configured).resolve()
    metadata = core.with_suffix('.nsmeta')
    if not core.is_file() or not metadata.is_file():
        print('FAIL: configured Core or metadata is missing', file=sys.stderr)
        return 1
    temp = os.environ.get('NSC_TEST_TEMP_ROOT') or os.environ.get('TEMP')
    if not temp or not Path(temp).is_dir() or Path(temp).resolve().drive.upper() != 'A:':
        print('SKIP: temporary directory must exist on A:')
        return 77
    def hashes():
        return {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in (compiler, core, metadata)}
    before = hashes()
    print('SHA256 BEFORE', before, flush=True)
    failures = []
    template = '''using NativeSharp.Core;
class Trace { public static int OwnerDrops; public static int FieldDrops; }
class Resource { public Resource() {} ~Resource() { Trace.FieldDrops = Trace.FieldDrops + 1; } }
class Gate { static Gate() throws Exception { throw new Exception(); } public static int Read() { return 1; } }
KIND Payload { private owned Resource part; public Payload() { part = new Resource(); }
 ~Payload() { Trace.OwnerDrops = Trace.OwnerDrops + 1; BODY } }
class Program { public static int Main() {
 WEAK<Payload> weak; int caught = 0;
 try { STRONG<Payload> owner = SharedPointers.MakeSTRONG<Payload>(); weak = owner.Downgrade(); }
 catch (Exception error) { caught = caught + 1; }
 if (caught != EXPECT || Trace.OwnerDrops != 1 || Trace.FieldDrops != 1) return 1;
 if (!weak.Expired() || weak.WeakCount() != 1l) return 2;
 STRONG<Payload> restored = weak.Upgrade(); if (!restored.IsEmpty()) return 3;
 return 42;
} }'''
    with tempfile.TemporaryDirectory(prefix='nsc-shared-owned-drop-', dir=temp) as temporary:
        root = Path(temporary)
        env = os.environ.copy()
        for key in ('TEMP', 'TMP', 'APPDATA', 'LOCALAPPDATA'): env[key] = str(root)
        def invoke(command):
            command = [str(item) for item in command]
            result = subprocess.run(command, cwd=repo, env=env, capture_output=True,
                                    text=True, errors='replace', timeout=120)
            print('CWD', repo, 'COMMAND', subprocess.list2cmdline(command), 'EXIT',
                  result.returncode, result.stdout + result.stderr, flush=True)
            return result.returncode
        def attr(path): return escape(path.as_posix(), {'"': '&quot;'})
        for family in ('Rc', 'Arc'):
            for kind in ('class', 'struct'):
                for throwing in (False, True):
                    folder = root / (family + '-' + kind + ('-throw' if throwing else '-normal'))
                    folder.mkdir()
                    source = template.replace('WEAK', 'Weak' + family).replace('STRONG', family).replace('KIND', kind)
                    source = source.replace('BODY', 'int ignored = Gate.Read();' if throwing else '')
                    source = source.replace('EXPECT', '1' if throwing else '0')
                    (folder / 'Main.ns').write_text(source, encoding='utf-8')
                    project = folder / 'Main.nsproj'
                    project.write_text('<Project><PropertyGroup><Name>OwnedFieldProbe</Name></PropertyGroup><ItemGroup>'
                        '<Compile Include="Main.ns"/><LibraryReference Include="' + attr(core) + '" Metadata="' +
                        attr(metadata) + '"/></ItemGroup></Project>', encoding='utf-8')
                    for level in ('O0', 'O2'):
                        label = (family, kind, throwing, level)
                        exe = folder / (level + '.exe')
                        if invoke([compiler, '--emit-project-oop-exe', project, '-' + level,
                                   '-o', exe, '--no-artifact-cache']) != 0 or not exe.is_file():
                            failures.append(label + ('build',))
                        elif invoke([exe]) != 42:
                            failures.append(label + ('run',))
    after = hashes()
    print('SHA256 AFTER', after, flush=True)
    if before != after: failures.append(('input-hash-changed',))
    if failures: print('FAIL', failures, file=sys.stderr, flush=True)
    return int(bool(failures))


if __name__ == '__main__':
    sys.exit(main())
