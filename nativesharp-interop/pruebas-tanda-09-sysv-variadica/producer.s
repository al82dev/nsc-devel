# Control de los registros exigidos por SysV para esta firma variadica.
.text
.globl sysv_varargs
sysv_varargs:
    cmpl $3, %edi
    jne fail
    cmpl $-2, %esi
    jne fail
    movabsq $-5000000000, %r8
    cmpq %r8, %rdx
    jne fail
    movq %xmm0, %r8
    movabsq $0x4002000000000000, %r9
    cmpq %r9, %r8
    jne fail
    cmpb $1, %al
    jb fail
    movl $1, %eax
    ret
fail:
    xorl %eax, %eax
    ret
.globl sysv_many
sysv_many:
    cmpl $9, %edi
    jne fail_many
    cmpl $-7, %esi
    jne fail_many
    cmpl $250, %edx
    jne fail_many
    cmpl $-1000, %ecx
    jne fail_many
    cmpl $65000, %r8d
    jne fail_many
    movabsq $-5000000000, %r10
    cmpq %r10, %r9
    jne fail_many
    cmpl $4000000000, 8(%rsp)
    jne fail_many
    movq 16(%rsp), %r10
    testq %r10, %r10
    je fail_many
    cmpl $123, (%r10)
    jne fail_many
    movq %xmm0, %r10
    movabsq $0x4002000000000000, %r11
    cmpq %r11, %r10
    jne fail_many
    movq %xmm1, %r10
    movabsq $0x400c000000000000, %r11
    cmpq %r11, %r10
    jne fail_many
    cmpb $2, %al
    jb fail_many
    movl $1, %eax
    ret
fail_many:
    xorl %eax, %eax
    ret
