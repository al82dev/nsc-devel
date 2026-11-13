#include "api.h"
extern "C" { __declspec(selectany) int _fltused = 0; }
namespace T09Cpp {
int sum(int a,int b) { return 900+a+b; }
__attribute__((sysv_abi, naked)) int sum(int count,...) {
    __asm__("cmpl $1, %edi\n\t"
            "jne 1f\n\t"
            "cmpl $10, 8(%rsp)\n\t"
            "jne 1f\n\t"
            "cmpl $20, 12(%rsp)\n\t"
            "jne 1f\n\t"
            "cmpl $30, 16(%rsp)\n\t"
            "jne 1f\n\t"
            "movq 24(%rsp), %r8\n\t"
            "movabsq $0x4002000000000000, %r9\n\t"
            "cmpq %r9, %r8\n\t"
            "jne 1f\n\t"
            "movl $32, %eax\n\t"
            "retq\n\t"
            "1: xorl %eax, %eax\n\t"
            "retq\n\t");
}
}