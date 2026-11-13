"""Compara llamadas SysV con varios records y argumentos que pueden usar pila."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
args = parser.parse_args()
source = Path(__file__).resolve().parent
root = Path(tempfile.mkdtemp(prefix='nsc-t09-retorno-multiples-records-', dir=r'A:\temp'))
old_most = '?most@T09Cpp@@YA?AUSmall@1@U21@H0@Z'
old_fetch = '?fetchCallback@T09Cpp@@YAP6A?AUSmall@1@U21@H0@ZXZ'
print('Evidencia:', root, flush=True)
for count in (3, 6):
    dest = root / str(count)
    shutil.copytree(source, dest)
    header = dest / 'api.h'
    decl = 'Small, int' + ', Small' * (count - 1)
    header.write_text(header.read_text().replace('Small, int, Small', decl))
    impl = dest / 'api.cpp'
    params = 'Small p0, int bias' + ''.join(', Small p' + str(i) for i in range(1, count))
    checks = ' || '.join('p{0}.x!={1} || p{0}.y!={2} || p{0}.z!={3}'.format(i, i+1, i+10, i+20) for i in range(1, count))
    add_x = '+'.join('p' + str(i) + '.x' for i in range(1, count))
    add_z = '+'.join('p' + str(i) + '.z' for i in range(1, count))
    body = 'if (' + checks + ') { p0.z=-99; return p0; } p0.x+=' + add_x + '; p0.z+=bias+' + add_z + '; return p0;'
    impl.write_text('extern "C" { __declspec(selectany) int _fltused = 0; }\n#include "api.h"\nnamespace T09Cpp {\nSmall __attribute__((sysv_abi)) most(' + params + ') { ' + body + ' }\nCallback fetchCallback() { return most; }\n}\n')
    ns_init = ' '.join('T09Cpp.Small p{0}; p{0}.x={1}; p{0}.y={2}; p{0}.z={3};'.format(i, i+1, i+10, i+20) for i in range(count))
    cpp_init = ' '.join('T09Cpp::Small p{0}{{{1},{2},{3}}};'.format(i, i+1, i+10, i+20) for i in range(count))
    arguments = 'p0,5' + ''.join(',p' + str(i) for i in range(1, count))
    originals = ' && '.join('p{0}.x=={1} && p{0}.y=={2} && p{0}.z=={3}'.format(i, i+1, i+10, i+20) for i in range(count))
    x = sum(range(1, count + 1))
    z = 20 + 5 + sum(i+20 for i in range(1, count))
    result = 'a.x=={0} && a.y==10 && a.z=={1} && b.x=={0} && b.y==10 && b.z=={1}'.format(x, z)
    ns_body = ns_init + ' T09Cpp.Callback callback=T09Cpp.fetchCallback(); T09Cpp.Callback direct=T09Cpp.most; T09Cpp.Small a=T09Cpp.most(' + arguments + '); T09Cpp.Small b=callback(' + arguments + '); return callback==direct && ' + originals + ' && ' + result + ' ? 0 : 1;'
    (dest / 'Main.ns').write_text('int Main() { ' + ns_body + ' }\n')
    (dest / 'Producer.ns').write_text('namespace T09 { public class RegBridge { public static int Run<T>(T unused) { ' + ns_body + ' } } }\n')
    (dest / 'Control.cpp').write_text('extern "C" { __declspec(selectany) int _fltused = 0; }\n#include "api.h"\nint main() { ' + cpp_init + ' auto callback=T09Cpp::fetchCallback(); auto a=T09Cpp::most(' + arguments + '); auto b=callback(' + arguments + '); return callback==&T09Cpp::most && ' + originals + ' && ' + result + ' ? 0 : 1; }\n')
    obj = dest / 'probe.obj'
    for command in (['clang.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-c', str(impl), '-o', str(obj)],
                    ['llvm-nm.exe', '--defined-only', str(obj)]):
        output = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace')
        if output.returncode: raise SystemExit(str(count) + ': ' + output.stdout)
    symbols = [line.split()[-1] for line in output.stdout.splitlines() if line.split()]
    most = [s for s in symbols if s.startswith('?most@T09Cpp@@')]
    fetch = [s for s in symbols if s.startswith('?fetchCallback@T09Cpp@@')]
    if len(most) != 1 or len(fetch) != 1: raise SystemExit('Simbolos inesperados: ' + repr(symbols))
    for script in ('validar.py', 'validar-precompilado.py'):
        path = dest / script
        text = path.read_text().replace(old_most, most[0]).replace(old_fetch, fetch[0])
        if count == 6 and script == 'validar.py':
            marker = "        if ('" + fetch[0]
            assertion = ("        if not re.search(r'call\\s+x86_64_sysvcc[^\\n]*ptr byval\\(\\[12 x i8\\]\\) align 8', ir):\n"
                         "            raise SystemExit(prefix + ': falta copia byval en llamada indirecta; evidencia: ' + str(root))\n")
            if marker not in text: raise SystemExit('No se encontro la comprobacion LLVM')
            text = text.replace(marker, assertion + marker, 1)
        path.write_text(text)
        command = [sys.executable, str(path), '--nsc', args.nsc, '--clang-root', args.clang_root]
        result = subprocess.run(command, env=os.environ.copy(), stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace')
        (root / (str(count) + '-' + script + '.log')).write_text('COMMAND: ' + repr(command) + '\nEXIT: ' + str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
        print(count, script, 'EXIT', result.returncode, flush=True)
        if result.returncode:
            print(result.stdout, flush=True)
            raise SystemExit('Fallo; evidencia: ' + str(root))
print('OK: tres y seis records; evidencia:', root)
