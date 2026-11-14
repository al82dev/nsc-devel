import argparse, os, re, shutil, subprocess, sys, tempfile
from pathlib import Path
parser=argparse.ArgumentParser()
parser.add_argument('--nsc',required=True)
parser.add_argument('--clang-root',required=True)
parser.add_argument('--modo',type=int,choices=range(4),default=0)
parser.add_argument('--dinamico',action='store_true')
parser.set_defaults(agregado_opcional=False, fijos_estrechos=False)
args=parser.parse_args()
if args.agregado_opcional and args.fijos_estrechos:
 parser.error('seleccione un caso')
root=Path(tempfile.mkdtemp(prefix='nsc-t09-fixed-record-size-',dir='A:/temp'))
fixture=root/'fixture'
shutil.copytree(Path(__file__).resolve().parent, fixture)
def change(name,old,new):
 p=fixture/name; s=p.read_text(encoding='utf-8'); assert s.count(old)==1,(name,old,s.count(old)); p.write_text(s.replace(old,new),encoding='utf-8')
change('api.h','int __attribute__((sysv_abi)) sum(int count,...)','int __attribute__((sysv_abi)) sum(int* __attribute__((pass_object_size(0))) count,T09VarValue fixed' + (',signed char marker,bool flag' if args.fijos_estrechos else '') + ',...)')
change('api.cpp','int sum(int count,...)','int sum(int* const __attribute__((pass_object_size(0))) count,T09VarValue fixed' + (',signed char marker,bool flag' if args.fijos_estrechos else '') + ',...)')
change('api.cpp','cmpl $1, %edi','cmpl $1, (%rdi)\\n\\t"\n            "jne 1f\\n\\t"\n            "cmpq $4, %rsi')
if args.agregado_opcional:
 check = 'cmpl $7, %ecx'
elif args.fijos_estrechos:
 check = 'cmpb $-4, %cl\\n\\t"\n            "jne 1f\\n\\t"\n            "cmpb $1, %r8b\\n\\t"\n            "jne 1f\\n\\t"\n            "cmpl $3, %r9d'
else:
 check = 'cmpl $3, %ecx'
change('api.cpp','cmpl $7, %esi','cmpl $7, %edx\\n\\t"\n            "jne 1f\\n\\t"\n            "' + check)
if args.agregado_opcional:
 change('api.cpp','cmpb $1, %al','movq %xmm1, %r8\\n\\t"\n            "cmpq %r9, %r8\\n\\t"\n            "jne 1f\\n\\t"\n            "cmpb $2, %al')
for name in ('Main.ns','Producer.ns'):
 change(name,'return T09Cpp.sum(1,v)','int count=1; return T09Cpp.sum(&count,v,' + ('v)' if args.agregado_opcional else '(sbyte)-4,true,3)' if args.fijos_estrechos else '3)'))
change('Control.cpp','return T09Cpp::sum(1,v)','int count=1; return T09Cpp::sum(&count,v,' + ('v)' if args.agregado_opcional else '-4,true,3)' if args.fijos_estrechos else '3)'))
change('api.h','T09VarValue fixed,...)','T09VarValue fixed,int last)')
change('api.cpp','T09VarValue fixed,...)','T09VarValue fixed,int last)')
change('api.cpp','            "cmpb $1, %al\\n\\t"\n            "jb 1f\\n\\t"\n','')
attribute='pass_dynamic_object_size' if args.dinamico else 'pass_object_size'
for name in ('api.h','api.cpp'):
 change(name,'pass_object_size(0)',attribute+'('+str(args.modo)+')')
def run(label,cmd):
 p=subprocess.run(cmd,cwd='A:/nsc-devel',env=os.environ.copy(),stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,errors='replace'); (root/(label+'.log')).write_text(p.stdout,encoding='utf-8'); print(label,'exit',p.returncode,'\n',p.stdout[-2500:],flush=True); return p
run('clang',['clang.exe','-target','x86_64-pc-windows-msvc','-std=c++20','-c',str(fixture/'api.cpp'),'-o',str(root/'api.obj')])
p=run('symbols',['llvm-nm.exe','--defined-only',str(root/'api.obj')]); names=[line.split()[-1] for line in p.stdout.splitlines() if '?sum@T09Cpp@@' in line and '__'+attribute+str(args.modo) in line]; assert len(names)==1
for name in ('validar.py','validar-precompilado.py'):
 p=fixture/name; s=p.read_text(encoding='utf-8').replace('?sum@T09Cpp@@YAHHZZ',names[0]); s=s.replace(r'i32\s+\(i32,\s*\.\.\.\)',r'i32\s+'); p.write_text(s,encoding='utf-8')
p=run('direct',['python.exe',str(fixture/'validar.py'),'--nsc',args.nsc,'--clang-root',args.clang_root])
if p.returncode: raise SystemExit(p.returncode)
match=re.search(r'Evidencia:\s*(.+)',p.stdout)
if not match: raise SystemExit('falta evidencia del IR')
direct=Path(match.group(1).strip())
for mode in ('estatica','dll'):
 for opt in range(4):
  ir=(direct/(mode+'-O'+str(opt)+'-ns-ir.log')).read_text(encoding='utf-8')
  lines=[line for line in ir.splitlines() if names[0] in line]
  if len(lines)!=2 or not any(re.search(r'\(ptr, i64, double, i32, i32\)', line) for line in lines) or any('...' in line for line in lines):
   raise SystemExit('firma fija incorrecta; evidencia: '+str(direct))
if args.fijos_estrechos:
 match=re.search(r'Evidencia:\s*(.+)',p.stdout)
 if not match: raise SystemExit('falta evidencia del IR')
 direct=Path(match.group(1).strip())
 for mode in ('estatica','dll'):
  for opt in range(4):
   ir=(direct/(mode+'-O'+str(opt)+'-ns-ir.log')).read_text(encoding='utf-8')
   lines=[line for line in ir.splitlines() if names[0] in line]
   if len(lines)!=2 or not all('i8 signext' in line and 'i1 zeroext' in line for line in lines):
    raise SystemExit('atributos fijos incorrectos; evidencia: '+str(direct))
p=run('precompiled',['python.exe',str(fixture/'validar-precompilado.py'),'--nsc',args.nsc,'--clang-root',args.clang_root])
print('EVIDENCE',root)
sys.exit(p.returncode)
