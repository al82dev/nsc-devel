import argparse,os,re,shutil,subprocess,sys,tempfile
from pathlib import Path
parser=argparse.ArgumentParser()
parser.add_argument('--nsc',required=True)
parser.add_argument('--clang-root',required=True)
parser.set_defaults(enteros=False,un_entero=False,vector=False,vector_mixto=False,fijo=True,entero32=False,flotante=False)
args=parser.parse_args()
if sum((args.enteros,args.un_entero,args.vector,args.vector_mixto,args.entero32,args.flotante))>1: parser.error('seleccione un caso')
root=Path(tempfile.mkdtemp(prefix='nsc-t09-sysv-variadic-sret-',dir='A:/temp')); fixture=root/'fixture';shutil.copytree(Path(__file__).resolve().parent,fixture)
def replace(name,before,after):
 p=fixture/name;s=p.read_text(encoding='utf-8');assert s.count(before)==1,(name,before,s.count(before));p.write_text(s.replace(before,after),encoding='utf-8')
replace('api.h','struct T09VarValue { double x; int y; };','struct T09VarValue { double x; int y; };\nstruct T09Return { double a; int b; };')
if args.entero32 or args.flotante:
 replace('api.h','struct T09Return { double a; int b; };','struct T09Return { float a; };' if args.flotante else 'struct T09Return { int a; };')
if args.vector or args.vector_mixto:
 replace('api.h','struct T09Return { double a; int b; };','struct T09Return { float a; float b; double c; };' if args.vector_mixto else 'struct T09Return { float a; float b; };')
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
if args.entero32: body=body.replace('movl $20, %eax','movl $10, %eax')
if args.flotante: body=body.replace('movl $20, %eax','movl $0x3f800000, %eax\\n\\t"\n            "movd %eax, %xmm0')
if args.fijo:
 body=body.replace('cmpl $7, %esi','cmpl $7, %esi\\n\\t"\n            "jne 1f\\n\\t"\n            "cmpl $3, %edx')
if args.vector or args.vector_mixto:
 value='movabsq $0x400000003f800000, %rax\\n\\t"\n            "movq %rax, %xmm0'
 if args.vector_mixto: value+='\\n\\t"\n            "movabsq $0x4008000000000000, %rax\\n\\t"\n            "movq %rax, %xmm1'
 body=body.replace('movl $20, %eax',value)
if args.enteros or args.un_entero:
 body=body.replace('movl $20, %eax','movq $10, %rax' if args.un_entero else 'movq $10, %rax\\n\\t"\n            "movq $20, %rdx')
p.write_text(s[:start]+body+s[end:],encoding='utf-8')
if args.fijo:
 replace('api.h','sum(int count,...)','sum(int count,T09VarValue fixed,...)')
 replace('api.cpp','sum(int count,...)','sum(int count,T09VarValue fixed,...)')
expected='r.a==1.0f' if args.flotante else 'r.a==10' if args.entero32 else 'r.a==1.0f && r.b==2.0f && r.c==3.0' if args.vector_mixto else 'r.a==1.0f && r.b==2.0f' if args.vector else 'r.a==10' if args.un_entero else 'r.a==10 && r.b==20' if args.enteros else 'r.a==2.25 && r.b==20'
for name in ('Main.ns','Producer.ns'):
 replace(name,'return T09Cpp.sum(1,v)==32 && v.x==2.25 && v.y==7 ? 0 : 1;', ('T09Return r=T09Cpp.sum(1,v,3); return ' if args.fijo else 'T09Return r=T09Cpp.sum(1,v); return ')+expected+' && v.x==2.25 && v.y==7 ? 0 : 1;')
replace('Control.cpp','return T09Cpp::sum(1,v)==32 && v.x==2.25 && v.y==7 ? 0 : 1;', ('T09Return r=T09Cpp::sum(1,v,3); return ' if args.fijo else 'T09Return r=T09Cpp::sum(1,v); return ')+expected+' && v.x==2.25 && v.y==7 ? 0 : 1;')
replace('api.h','sum(int count,T09VarValue fixed,...)','sum(int* __attribute__((pass_object_size(0))) count,T09VarValue fixed,int last)')
replace('api.cpp','sum(int count,T09VarValue fixed,...)','sum(int* const __attribute__((pass_object_size(0))) count,T09VarValue fixed,int last)')
replace('api.cpp','cmpl $1, %edi','cmpl $1, (%rdi)\\n\\t"\n            "jne 1f\\n\\t"\n            "cmpq $4, %rsi')
replace('api.cpp','cmpl $7, %esi','cmpl $7, %edx')
replace('api.cpp','cmpl $3, %edx','cmpl $3, %ecx')
for name in ('Main.ns','Producer.ns'):
 replace(name,'T09Return r=T09Cpp.sum(1,v,3);','int count=1; T09Return r=T09Cpp.sum(&count,v,3);')
replace('Control.cpp','T09Return r=T09Cpp::sum(1,v,3);','int count=1; T09Return r=T09Cpp::sum(&count,v,3);')
env=os.environ.copy();env['TEMP']=env['TMP']='A:/temp/t09-session-root';env['NSC_LINKER']='nslink.exe';env['NSC_WINDOWS_RUNTIME_LIBRARY']='A:/nsc-devel/build-tanda09/generated/NativeSharp.Runtime.Win32.lib'
def run(label,cmd):
 p=subprocess.run(cmd,cwd='A:/nsc-devel',env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,errors='replace');(root/(label+'.log')).write_text(p.stdout,encoding='utf-8');print(label,p.returncode,p.stdout[-2000:],flush=True);return p
p=run('clang',['clang.exe','-target','x86_64-pc-windows-msvc','-std=c++20','-c',str(fixture/'api.cpp'),'-o',str(root/'api.obj')]);assert p.returncode==0
p=run('symbols',['llvm-nm.exe','--defined-only',str(root/'api.obj')]);names=[line.split()[-1] for line in p.stdout.splitlines() if '?sum@T09Cpp@@' in line and 'UT09Return' in line];assert len(names)==1
for name in ('validar.py','validar-precompilado.py'):
 p=fixture/name;s=p.read_text(encoding='utf-8').replace('?sum@T09Cpp@@YAHHZZ',names[0]);s=s.replace(r'i32\s+\(i32,\s*\.\.\.\)',(r'float\s+\(i32,\s*\.\.\.\)' if args.flotante else r'i32\s+\(i32,\s*\.\.\.\)' if args.entero32 else r'\{\s*<2 x float>,\s*double\s*\}\s+\(i32,\s*\.\.\.\)' if args.vector_mixto else r'<2 x float>\s+\(i32,\s*\.\.\.\)' if args.vector else r'i64\s+\(i32,\s*\.\.\.\)' if args.un_entero else r'\{\s*i64,\s*i64\s*\}\s+\(i32,\s*\.\.\.\)' if args.enteros else r'\{\s*double,\s*i32\s*\}\s+\(i32,\s*\.\.\.\)'));s=s.replace(r'\(i32,\s*\.\.\.\)',r'\(i32,\s*double,\s*i32,\s*\.\.\.\)') if args.fijo else s;s=s.replace(r'\{\s*double,\s*i32\s*\}\s+\(i32,\s*double,\s*i32,\s*\.\.\.\)',r'\{\s*double,\s*i32\s*\}\s+') if args.fijo else s;p.write_text(s,encoding='utf-8')
p=run('direct',['python.exe',str(fixture/'validar.py'),'--nsc',args.nsc,'--clang-root',args.clang_root])
if p.returncode: raise SystemExit(p.returncode)
match=re.search(r'Evidencia:\s*(.+)',p.stdout)
if not match: raise SystemExit('falta evidencia del IR')
direct=Path(match.group(1).strip())
for mode in ('estatica','dll'):
 for opt in range(4):
  ir=(direct/(mode+'-O'+str(opt)+'-ns-ir.log')).read_text(encoding='utf-8')
  lines=[line for line in ir.splitlines() if names[0] in line]
  physical='float' if args.flotante else 'i32' if args.entero32 else '{ <2 x float>, double }' if args.vector_mixto else '<2 x float>' if args.vector else 'i64' if args.un_entero else '{ i64, i64 }' if args.enteros else '{ double, i32 }'
  if len(lines)!=2 or not all(physical in line and 'sret(' not in line for line in lines) or (not args.un_entero and not args.entero32 and not args.flotante and not args.vector and 'extractvalue '+physical not in ir): raise SystemExit('retorno directo incorrecto; evidencia: '+str(direct))
p=run('precompiled',['python.exe',str(fixture/'validar-precompilado.py'),'--nsc',args.nsc,'--clang-root',args.clang_root])
print('EVIDENCE',root);sys.exit(p.returncode)
