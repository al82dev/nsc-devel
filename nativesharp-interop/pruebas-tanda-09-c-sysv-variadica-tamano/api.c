#include "api.h"
__declspec(selectany) int _fltused = 0;
__attribute__((sysv_abi, naked)) int t09_csum(
    int *const p __attribute__((pass_object_size(0))), signed char marker, _Bool flag, ...) {
    __asm__("cmpl $3, (%rdi)\n\t"
            "jne 1f\n\t"
            "cmpq $4, %rsi\n\t"
            "jne 1f\n\t"
            "cmpb $-4, %dl\n\t"
            "jne 1f\n\t"
            "cmpb $1, %cl\n\t"
            "jne 1f\n\t"
            "cmpl $-3, %r8d\n\t"
            "jne 1f\n\t"
            "cmpl $7, %r9d\n\t"
            "jne 1f\n\t"
            "movq %xmm0, %r8\n\t"
            "movabsq $0x4004000000000000, %r9\n\t"
            "cmpq %r9, %r8\n\t"
            "jne 1f\n\t"
            "cmpb $1, %al\n\t"
            "jb 1f\n\t"
            "movl $32, %eax\n\t"
            "retq\n\t"
            "1: xorl %eax, %eax\n\t"
            "retq\n\t");
}
