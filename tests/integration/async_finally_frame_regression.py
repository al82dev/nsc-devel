"""Exercise synchronous nested finally/catch state across SEH cleanup."""
from pathlib import Path
import os
import re
import subprocess
import sys
import tempfile

import async_catch_owner_continuation as common
import async_finally_exception_replacement as replacement


original = replacement.common.CASES
common.CASES = {
    # Keep the original controls and reproduction verbatim.
    'sync_preserve_original': original['sync_preserve'],
    'sync_inner_catch_original': original['sync_inner_catch'],
    'sync_inner_catch_parameter': '''public static int Work(int value) throws Exception {
 try { throw new Exception(1); } finally {
  try { throw new Exception(2); } catch (Exception inner) { value = value + inner.Id; }
  Counter.Log = value;
 }
} public static async int Main() { int caught = 0;
 try { int result = Work(39); } catch (Exception error) { caught = error.Id; }
 return caught == 1 && Counter.Log == 41 && Exception.Drops == 2 && Exception.Log == 21 ? 42 : 8; }''',
    'sync_inner_catch_forceinline': '''public static forceinline int Work(int value) throws Exception {
 try { throw new Exception(1); } finally {
  try { throw new Exception(2); } catch (Exception inner) { value = value + inner.Id; }
  Counter.Log = value;
 }
} public static async int Main() { int caught = 0;
 try { int result = Work(39); } catch (Exception error) { caught = error.Id; }
 return caught == 1 && Counter.Log == 41 && Exception.Drops == 2 && Exception.Log == 21 ? 42 : 9; }''',
}


def check_forceinline_ir():
    repository = Path(__file__).resolve().parents[2]
    compiler = Path(sys.argv[1]).resolve()
    symbol = '__nsc_method_Probe$3A$3AProgram__Work__builtin$3Aint'
    caller = '__nsc_method_Probe$3A$3AProgram__Main$poll'
    call = re.compile(r'\b(?:call|invoke)\b[^\n]*@"?' + re.escape(symbol) + r'"?\(')
    caller_definition = re.compile(
        r'^define\b[^\n]*@"?' + re.escape(caller) + r'"?\([^\n]*\)[^\n]*\{\s*$\n(.*?)^\}',
        re.MULTILINE | re.DOTALL)
    with tempfile.TemporaryDirectory(prefix='nsc-finally-frame-ir-') as temporary:
        root = Path(temporary)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup>'
                           '<ItemGroup><Compile Include="Main.ns" /></ItemGroup></Project>', encoding='utf-8')
        (root / 'Main.ns').write_text(
            common.PREFIX.replace('BODY', common.CASES['sync_inner_catch_forceinline']), encoding='utf-8')
        mir = subprocess.run(
            [str(compiler), '--dump-project-oop-mir', str(project)],
            cwd=repository, capture_output=True, text=True, errors='replace', timeout=120)
        required = re.compile(r'^function #\d+\s+' + re.escape(symbol) + r'\s+inlining=forceinline\b', re.MULTILINE)
        if mir.returncode != 0 or len(required.findall(mir.stdout)) != 1:
            print('FAIL forceinline-ir cannot identify Work in MIR', mir.stdout + mir.stderr, file=sys.stderr)
            return 1
        raw = subprocess.run(
            [str(compiler), '--dump-project-oop-llvm', str(project)],
            cwd=repository, capture_output=True, text=True, errors='replace', timeout=120)
        raw_caller = caller_definition.search(raw.stdout)
        if raw.returncode != 0 or raw_caller is None or not call.search(raw_caller.group(1)):
            print('FAIL forceinline-ir missing direct-call control', raw.stdout + raw.stderr, file=sys.stderr)
            return 1
        for level in ('O0', 'O2'):
            result = subprocess.run(
                [str(compiler), '--dump-project-oop-llvm-opt', str(project), '-' + level],
                cwd=repository, capture_output=True, text=True, errors='replace', timeout=120)
            optimized_caller = caller_definition.search(result.stdout)
            if result.returncode != 0 or optimized_caller is None:
                print('FAIL forceinline-ir', level, result.stdout + result.stderr, file=sys.stderr)
                return 1
            if call.search(optimized_caller.group(1)):
                print('FAIL forceinline-ir', level, 'direct call was not expanded', file=sys.stderr)
                return 1
            print('forceinline-ir', level, 'no direct call to Work', flush=True)
    return 0


def main():
    if os.name != 'nt':
        print('SKIP synchronous SEH cleanup requires Windows', file=sys.stderr)
        return 77
    result = common.main()
    return result if result != 0 else check_forceinline_ir()


if __name__ == '__main__':
    sys.exit(main())
