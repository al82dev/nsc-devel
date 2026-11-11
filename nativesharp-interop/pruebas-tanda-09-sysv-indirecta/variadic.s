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
.globl t09_variadic_pressure
t09_variadic_pressure:
    cmpl $18, %edi
    jne fail_pressure
    cmpb $8, %al
    jne fail_pressure
    cmpl $1, %esi
    jne fail_pressure
    cmpl $2, %edx
    jne fail_pressure
    cmpl $3, %ecx
    jne fail_pressure
    cmpl $4, %r8d
    jne fail_pressure
    cmpl $5, %r9d
    jne fail_pressure
    cmpl $6, 8(%rsp)
    jne fail_pressure
    cmpl $-294967296, 16(%rsp)
    jne fail_pressure
    movq %xmm0, %r10
    movabsq $0x3ff0000000000000, %r11
    cmpq %r11, %r10
    jne fail_pressure
    movq %xmm1, %r10
    movabsq $0x4000000000000000, %r11
    cmpq %r11, %r10
    jne fail_pressure
    movq %xmm2, %r10
    movabsq $0x4008000000000000, %r11
    cmpq %r11, %r10
    jne fail_pressure
    movq %xmm3, %r10
    movabsq $0x4010000000000000, %r11
    cmpq %r11, %r10
    jne fail_pressure
    movq %xmm4, %r10
    movabsq $0x4014000000000000, %r11
    cmpq %r11, %r10
    jne fail_pressure
    movq %xmm5, %r10
    movabsq $0x4018000000000000, %r11
    cmpq %r11, %r10
    jne fail_pressure
    movq %xmm6, %r10
    movabsq $0x401c000000000000, %r11
    cmpq %r11, %r10
    jne fail_pressure
    movq %xmm7, %r10
    movabsq $0x4020000000000000, %r11
    cmpq %r11, %r10
    jne fail_pressure
    movabsq $0x4022000000000000, %r11
    cmpq %r11, 24(%rsp)
    jne fail_pressure
    movq 32(%rsp), %r10
    cmpl $17, (%r10)
    jne fail_pressure
    cmpl $1, 40(%rsp)
    jne fail_pressure
    movl $43, (%r10)
    movl $1, %eax
    ret
fail_pressure:
    xorl %eax, %eax
    ret
