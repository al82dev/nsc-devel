#include "api.h"
extern "C" { __declspec(selectany) int _fltused = 0; }
namespace T09Cpp {
int sum(int a,int b) { return 900+a+b; }
__attribute__((sysv_abi,naked)) int sum(int count,...) { __asm__("cmpl $1, %edi\n\tjne 1f\n\tcmpw $-1, %si\n\tjne 1f\n\tcmpl $8, %edx\n\tjne 1f\n\tmovl $32, %eax\n\tretq\n\t1: xorl %eax, %eax\n\tretq\n\t"); }
Callback fetchCallback() { return static_cast<Callback>(sum); }
}
