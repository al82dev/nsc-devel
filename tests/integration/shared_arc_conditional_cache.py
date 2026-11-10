"""Directed cache and invalidation checks for conditional Arc thread traits."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    compiler = Path(sys.argv[1]).resolve()
    library = Path(sys.argv[2]).resolve()
    evidence = Path(sys.argv[3]).resolve()
    unmarked_library = Path(sys.argv[4]).resolve() if len(sys.argv) > 4 else None
    evidence.mkdir(parents=True, exist_ok=True)
    safe_source = '''using NativeSharp.Core;
struct Safe { public int Value; public Safe() { Value = 42; } }
class Program { static int RequireSend<T>() where T : Send { return 1; }
 static int RequireSync<T>() where T : Sync { return 1; }
 public static int Main() { Arc<Safe> owner = SharedPointers.MakeArc<Safe>();
 if (RequireSend<Arc<Safe>>() != 1 || RequireSync<Arc<Safe>>() != 1) return 1;
 return owner.Get().Value; } }
'''
    unsafe_source = '''using NativeSharp.Core;
struct Raw { public int* Value; }
class Program { static int RequireSend<T>() where T : Send { return 1; }
 public static int Main() { return RequireSend<Arc<Raw>>(); } }
'''
    results = []
    with tempfile.TemporaryDirectory(prefix='nsc-arc-conditional-cache-') as temporary:
        root = Path(temporary)
        local_library = root / 'NativeSharpCore.lib'
        def install_library(source_library):
            for suffix in ('.lib', '.nsmeta', '.nsproducts'):
                shutil.copyfile(source_library.with_suffix(suffix), local_library.with_suffix(suffix))
        install_library(library)
        source = root / 'Main.ns'
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>ArcConditionalCache</Name>'
                           '</PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
                           '<LibraryReference Include="' + local_library.as_posix() + '" Metadata="' +
                           local_library.with_suffix('.nsmeta').as_posix() + '" />'
                           '</ItemGroup></Project>', encoding='utf-8')
        executable = root / 'probe.exe'
        cache = root / 'cache'
        command = [str(compiler), '--emit-project-oop-exe', str(project), '-O0', '-o',
                   str(executable), '--artifact-cache=' + cache.as_posix(), '--time']
        cases = [('safe-cold', safe_source, True, False, library),
                 ('safe-warm', safe_source, True, True, library),
                 ('unsafe-changed', unsafe_source, False, False, library),
                 ('safe-restored', safe_source, True, True, library)]
        if unmarked_library is not None:
            cases.extend([('dependency-unmarked', safe_source, False, False, unmarked_library),
                          ('dependency-restored', safe_source, True, True, library)])
        installed = library
        for label, text, expected, hit, selected_library in cases:
            if selected_library != installed:
                install_library(selected_library)
                installed = selected_library
            source.write_text(text, encoding='utf-8')
            result = subprocess.run(command, cwd=root, capture_output=True, text=True,
                                    errors='replace', timeout=120)
            output = result.stdout + result.stderr
            (evidence / (label + '.log')).write_text(
                'COMMAND: ' + ' '.join(command) + '\n' + output + '\nEXIT: ' +
                str(result.returncode) + '\n', encoding='utf-8')
            cache_hit = 'compilation-bundle-cache: hit' in output
            row = {'case': label, 'compile_exit': result.returncode, 'bundle_hit': cache_hit,
                   'source_sha256': hashlib.sha256(text.encode()).hexdigest()}
            if expected:
                if result.returncode != 0 or cache_hit != hit:
                    print('FAIL', label, output, file=sys.stderr)
                    return 1
                execution = subprocess.run([str(executable)], cwd=root, capture_output=True,
                                           text=True, errors='replace', timeout=120)
                row['run_exit'] = execution.returncode
                if execution.returncode != 42:
                    print('FAIL', label, 'run', execution.returncode, execution.stdout,
                          execution.stderr, file=sys.stderr)
                    return 1
            elif result.returncode == 0 or 'does not implement it' not in output or cache_hit:
                print('FAIL', label, output, file=sys.stderr)
                return 1
            results.append(row)
            print(label, 'compile=' + str(result.returncode), 'bundle_hit=' + str(cache_hit),
                  'run=' + str(row.get('run_exit', 'not-run')), flush=True)
    (evidence / 'results.json').write_text(json.dumps({
        'compiler': str(compiler), 'compiler_sha256': sha256(compiler),
        'library': str(library), 'library_sha256': sha256(library),
        'metadata_sha256': sha256(library.with_suffix('.nsmeta')),
        'unmarked_library': str(unmarked_library) if unmarked_library else None,
        'unmarked_metadata_sha256': sha256(unmarked_library.with_suffix('.nsmeta')) if unmarked_library else None,
        'cases': results,
    }, indent=2), encoding='utf-8')
    return 0


if __name__ == '__main__':
    sys.exit(main())
