.text
.globl t09_variadic
t09_variadic:
    cmpl $3, %edi
    jne fail_variadic
    cmpl $-2, %esi
    jne fail_variadic
    cmpb $1, %al
    jne fail_variadic
    movq %xmm0, %r8
    movabsq $0x4002000000000000, %r9
    cmpq %r9, %r8
    jne fail_variadic
    cmpl $17, (%rdx)
    jne fail_variadic
    movl $42, (%rdx)
    movl $1, %eax
    ret
fail_variadic:
    xorl %eax, %eax
    ret
