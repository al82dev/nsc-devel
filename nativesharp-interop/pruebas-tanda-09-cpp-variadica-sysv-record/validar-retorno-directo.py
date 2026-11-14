import argparse,os,re,shutil,subprocess,sys,tempfile
from pathlib import Path
parser=argparse.ArgumentParser()
parser.add_argument('--nsc',required=True)
parser.add_argument('--clang-root',required=True)
parser.add_argument('--enteros',action='store_true')
parser.add_argument('--un-entero',action='store_true')
args=parser.parse_args()
if args.enteros and args.un_entero: parser.error('seleccione un caso')
root=Path(tempfile.mkdtemp(prefix='nsc-t09-sysv-variadic-sret-',dir='A:/temp')); fixture=root/'fixture';shutil.copytree(Path(__file__).resolve().parent,fixture)
def replace(name,before,after):
 p=fixture/name;s=p.read_text(encoding='utf-8');assert s.count(before)==1,(name,before,s.count(before));p.write_text(s.replace(before,after),encoding='utf-8')
replace('api.h','struct T09VarValue { double x; int y; };','struct T09VarValue { double x; int y; };\nstruct T09Return { double a; int b; };')
if args.enteros or args.un_entero:
 replace('api.h','struct T09Return { double a; int b; };','struct T09Return { long long a; };' if args.un_entero else 'struct T09Return { long long a; long long b; };')
replace('api.h','int __attribute__((sysv_abi)) sum(int count,...)','T09Return __attribute__((sysv_abi)) sum(int count,...)')
replace('api.cpp','int sum(int count,...)','T09Return sum(int count,...)')
p=fixture/'api.cpp';s=p.read_text(encoding='utf-8');start=s.index('    __asm__(');end=s.index(');',start)+2
body=r'''    __asm__("cmpl $1, %edi\n\t"
            "jne 1f\n\t"
            "cmpl $7, %esi\n\t"
            "jne 1f\n\t"
            "movq %xmm0, %r8\n\t"
            "movabsq $0x4002000000000000, %r9\n\t"
            "cmpq %r9, %r8\n\t"
            "jne 1f\n\t"
            "movl $20, %eax\n\t"
            "retq\n\t"
            "1: xorl %eax, %eax\n\t"
            "retq\n\t");'''
if args.enteros or args.un_entero:
 body=body.replace('movl $20, %eax','movq $10, %rax' if args.un_entero else 'movq $10, %rax\\n\\t"\n            "movq $20, %rdx')
p.write_text(s[:start]+body+s[end:],encoding='utf-8')
expected='r.a==10' if args.un_entero else 'r.a==10 && r.b==20' if args.enteros else 'r.a==2.25 && r.b==20'
for name in ('Main.ns','Producer.ns'):
 replace(name,'return T09Cpp.sum(1,v)==32 && v.x==2.25 && v.y==7 ? 0 : 1;', 'T09Return r=T09Cpp.sum(1,v); return '+expected+' && v.x==2.25 && v.y==7 ? 0 : 1;')
replace('Control.cpp','return T09Cpp::sum(1,v)==32 && v.x==2.25 && v.y==7 ? 0 : 1;', 'T09Return r=T09Cpp::sum(1,v); return '+expected+' && v.x==2.25 && v.y==7 ? 0 : 1;')
env=os.environ.copy();env['TEMP']=env['TMP']='A:/temp/t09-session-root';env['NSC_LINKER']='nslink.exe';env['NSC_WINDOWS_RUNTIME_LIBRARY']='A:/nsc-devel/build-tanda09/generated/NativeSharp.Runtime.Win32.lib'
def run(label,cmd):
 p=subprocess.run(cmd,cwd='A:/nsc-devel',env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,errors='replace');(root/(label+'.log')).write_text(p.stdout,encoding='utf-8');print(label,p.returncode,p.stdout[-2000:],flush=True);return p
p=run('clang',['clang.exe','-target','x86_64-pc-windows-msvc','-std=c++20','-c',str(fixture/'api.cpp'),'-o',str(root/'api.obj')]);assert p.returncode==0
p=run('symbols',['llvm-nm.exe','--defined-only',str(root/'api.obj')]);names=[line.split()[-1] for line in p.stdout.splitlines() if '?sum@T09Cpp@@' in line and 'UT09Return' in line];assert len(names)==1
for name in ('validar.py','validar-precompilado.py'):
 p=fixture/name;s=p.read_text(encoding='utf-8').replace('?sum@T09Cpp@@YAHHZZ',names[0]);s=s.replace(r'i32\s+\(i32,\s*\.\.\.\)',(r'i64\s+\(i32,\s*\.\.\.\)' if args.un_entero else r'\{\s*i64,\s*i64\s*\}\s+\(i32,\s*\.\.\.\)' if args.enteros else r'\{\s*double,\s*i32\s*\}\s+\(i32,\s*\.\.\.\)'));p.write_text(s,encoding='utf-8')
p=run('direct',['python.exe',str(fixture/'validar.py'),'--nsc',args.nsc,'--clang-root',args.clang_root])
if p.returncode: raise SystemExit(p.returncode)
match=re.search(r'Evidencia:\s*(.+)',p.stdout)
if not match: raise SystemExit('falta evidencia del IR')
direct=Path(match.group(1).strip())
for mode in ('estatica','dll'):
 for opt in range(4):
  ir=(direct/(mode+'-O'+str(opt)+'-ns-ir.log')).read_text(encoding='utf-8')
  lines=[line for line in ir.splitlines() if names[0] in line]
  physical='i64' if args.un_entero else '{ i64, i64 }' if args.enteros else '{ double, i32 }'
  if len(lines)!=2 or not all(physical in line and 'sret(' not in line for line in lines) or (not args.un_entero and 'extractvalue '+physical not in ir): raise SystemExit('retorno directo incorrecto; evidencia: '+str(direct))
p=run('precompiled',['python.exe',str(fixture/'validar-precompilado.py'),'--nsc',args.nsc,'--clang-root',args.clang_root])
print('EVIDENCE',root);sys.exit(p.returncode)
