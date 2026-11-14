import argparse,os,re,shutil,subprocess,sys,tempfile
from pathlib import Path
parser=argparse.ArgumentParser()
parser.add_argument('--nsc',required=True)
parser.add_argument('--clang-root',required=True)
parser.add_argument('--escalar',action='store_true')
parser.add_argument('--tamano',action='store_true')
parser.add_argument('--sin-opcionales',action='store_true')
args=parser.parse_args()
if sum((args.escalar,args.tamano,args.sin_opcionales)) > 1: parser.error('seleccione un caso')
root=Path(tempfile.mkdtemp(prefix='nsc-t09-sysv-variadic-sret-',dir='A:/temp')); fixture=root/'fixture';shutil.copytree(Path(__file__).resolve().parent,fixture)
def replace(name,before,after):
 p=fixture/name;s=p.read_text(encoding='utf-8');assert s.count(before)==1,(name,before,s.count(before));p.write_text(s.replace(before,after),encoding='utf-8')
replace('api.h','struct T09VarValue { double x; int y; };','struct T09VarValue { double x; int y; };\nstruct T09Return { long long a; long long b; long long c; };')
replace('api.h','int __attribute__((sysv_abi)) sum(int count,...)','T09Return __attribute__((sysv_abi)) sum(int count,...)')
replace('api.cpp','int sum(int count,...)','T09Return sum(int count,...)')
p=fixture/'api.cpp';s=p.read_text(encoding='utf-8');start=s.index('    __asm__(');end=s.index(');',start)+2
body=r'''    __asm__("cmpl $1, %esi\n\t"
            "jne 1f\n\t"
            "cmpl $7, %edx\n\t"
            "jne 1f\n\t"
            "movq %xmm0, %r8\n\t"
            "movabsq $0x4002000000000000, %r9\n\t"
            "cmpq %r9, %r8\n\t"
            "jne 1f\n\t"
            "movq $10, (%rdi)\n\t"
            "movq $20, 8(%rdi)\n\t"
            "movq $30, 16(%rdi)\n\t"
            "movq %rdi, %rax\n\t"
            "retq\n\t"
            "1: movq $0, (%rdi)\n\t"
            "movq %rdi, %rax\n\t"
            "retq\n\t");'''
if args.sin_opcionales:
 body=body.replace('            "cmpl $7, %edx\\n\\t"\n            "jne 1f\\n\\t"\n','')
if args.escalar or args.sin_opcionales:
 if args.escalar: body=body.replace('cmpl $7, %edx','cmpl $7, %edx\\n\\t"\n            "jne 1f\\n\\t"\n            "cmpl $8, %ecx')
 body=body.replace('            "movq %xmm0, %r8\\n\\t"\n            "movabsq $0x4002000000000000, %r9\\n\\t"\n            "cmpq %r9, %r8\\n\\t"\n            "jne 1f\\n\\t"\n','')
if args.tamano:
 body=body.replace('cmpl $1, %esi','cmpl $1, (%rsi)\\n\\t"\n            "jne 1f\\n\\t"\n            "cmpq $4, %rdx').replace('cmpl $7, %edx','cmpl $7, %ecx')
p.write_text(s[:start]+body+s[end:],encoding='utf-8')
if args.tamano:
 replace('api.h','sum(int count,...)','sum(int * __attribute__((pass_object_size(0))) count,...)')
 replace('api.cpp','sum(int count,...)','sum(int * const __attribute__((pass_object_size(0))) count,...)')
for name in ('Main.ns','Producer.ns'):
 call='T09Cpp.sum(1)' if args.sin_opcionales else 'T09Cpp.sum(1,7,8)' if args.escalar else 'T09Cpp.sum(&count,v)' if args.tamano else 'T09Cpp.sum(1,v)'
 replace(name,'return T09Cpp.sum(1,v)==32 && v.x==2.25 && v.y==7 ? 0 : 1;', ('int count=1; ' if args.tamano else '')+'T09Return r='+call+'; return r.a==10 && r.b==20 && r.c==30 && v.x==2.25 && v.y==7 ? 0 : 1;')
call='T09Cpp::sum(1)' if args.sin_opcionales else 'T09Cpp::sum(1,7,8)' if args.escalar else 'T09Cpp::sum(&count,v)' if args.tamano else 'T09Cpp::sum(1,v)'
replace('Control.cpp','return T09Cpp::sum(1,v)==32 && v.x==2.25 && v.y==7 ? 0 : 1;', ('int count=1; ' if args.tamano else '')+'T09Return r='+call+'; return r.a==10 && r.b==20 && r.c==30 && v.x==2.25 && v.y==7 ? 0 : 1;')
env=os.environ.copy();env['TEMP']=env['TMP']='A:/temp/t09-session-root';env['NSC_LINKER']='nslink.exe';env['NSC_WINDOWS_RUNTIME_LIBRARY']='A:/nsc-devel/build-tanda09/generated/NativeSharp.Runtime.Win32.lib'
def run(label,cmd):
 p=subprocess.run(cmd,cwd='A:/nsc-devel',env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,errors='replace');(root/(label+'.log')).write_text(p.stdout,encoding='utf-8');print(label,p.returncode,p.stdout[-2000:],flush=True);return p
p=run('clang',['clang.exe','-target','x86_64-pc-windows-msvc','-std=c++20','-c',str(fixture/'api.cpp'),'-o',str(root/'api.obj')]);assert p.returncode==0
p=run('symbols',['llvm-nm.exe','--defined-only',str(root/'api.obj')]);names=[line.split()[-1] for line in p.stdout.splitlines() if '?sum@T09Cpp@@' in line and 'UT09Return' in line];assert len(names)==1
for name in ('validar.py','validar-precompilado.py'):
 p=fixture/name;s=p.read_text(encoding='utf-8').replace('?sum@T09Cpp@@YAHHZZ',names[0]);s=s.replace(r'i32\s+\(i32,\s*\.\.\.\)',(r'void\s+\(ptr,\s*ptr,\s*i64,\s*\.\.\.\)' if args.tamano else r'void\s+\(ptr,\s*i32,\s*\.\.\.\)'));p.write_text(s,encoding='utf-8')
p=run('direct',['python.exe',str(fixture/'validar.py'),'--nsc',args.nsc,'--clang-root',args.clang_root])
if p.returncode: raise SystemExit(p.returncode)
match=re.search(r'Evidencia:\s*(.+)',p.stdout)
if not match: raise SystemExit('falta evidencia del IR')
direct=Path(match.group(1).strip())
for mode in ('estatica','dll'):
 for opt in range(4):
  ir=(direct/(mode+'-O'+str(opt)+'-ns-ir.log')).read_text(encoding='utf-8')
  lines=[line for line in ir.splitlines() if names[0] in line]
  if len(lines)!=2 or not all('sret([24 x i8])' in line and 'align 8' in line for line in lines): raise SystemExit('sret incorrecto; evidencia: '+str(direct))
p=run('precompiled',['python.exe',str(fixture/'validar-precompilado.py'),'--nsc',args.nsc,'--clang-root',args.clang_root])
print('EVIDENCE',root);sys.exit(p.returncode)
