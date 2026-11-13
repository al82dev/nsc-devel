#include "api.h"
extern "C" { __declspec(selectany) int _fltused = 0; }
namespace T09Cpp {
int sum(int a,int b) { return 900+a+b; }
__attribute__((sysv_abi, naked)) int sum(int count,...) {
    __asm__("cmpl $1, %edi\n\t"
            "jne 1f\n\t"
            "cmpw $-1, %si\n\t"
            "jne 1f\n\t"
            "cmpl $7, %edx\n\t"
            "jne 1f\n\t"
            "movq %xmm0, %r8\n\t"
            "movabsq $0x4002000000000000, %r9\n\t"
            "cmpq %r9, %r8\n\t"
            "jne 1f\n\t"
            "cmpb $1, %al\n\t"
            "jb 1f\n\t"
            "movl $32, %eax\n\t"
            "retq\n\t"
            "1: xorl %eax, %eax\n\t"
            "retq\n\t");
}
}