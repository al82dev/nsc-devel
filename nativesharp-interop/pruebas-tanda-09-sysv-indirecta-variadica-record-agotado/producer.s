.text
.globl t09_var_record
t09_var_record:
    cmpl $1, %edi
    jne fail_var_record
    testq %rsi, %rsi
    je fail_var_record
    cmpl $13, (%rsi)
    jne fail_var_record
    cmpb $8, %al
    jne fail_var_record
    movq 8(%rsp), %r8
    movabsq $0x4002000000000000, %r9
    cmpq %r9, %r8
    jne fail_var_record
    cmpl $7, 16(%rsp)
    jne fail_var_record
    movabsq $0x400c000000000000, %r9
    movq %xmm0, %r8
    cmpq %r9, %r8
    jne fail_var_record
    movq %xmm1, %r8
    cmpq %r9, %r8
    jne fail_var_record
    movq %xmm2, %r8
    cmpq %r9, %r8
    jne fail_var_record
    movq %xmm3, %r8
    cmpq %r9, %r8
    jne fail_var_record
    movq %xmm4, %r8
    cmpq %r9, %r8
    jne fail_var_record
    movq %xmm5, %r8
    cmpq %r9, %r8
    jne fail_var_record
    movq %xmm6, %r8
    cmpq %r9, %r8
    jne fail_var_record
    movq %xmm7, %r8
    cmpq %r9, %r8
    jne fail_var_record
    movl $99, 16(%rsp)
    movl $23, (%rsi)
    movl $1, %eax
    ret
fail_var_record:
    xorl %eax, %eax
    ret
