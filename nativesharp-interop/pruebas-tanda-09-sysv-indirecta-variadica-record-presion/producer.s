.text
.globl t09_var_record
t09_var_record:
    cmpl $1, %edi
    jne fail_var_record
    cmpl $7, %esi
    jne fail_var_record
    testq %rdx, %rdx
    je fail_var_record
    cmpl $13, (%rdx)
    jne fail_var_record
    cmpb $8, %al
    jne fail_var_record
    movq %xmm0, %r8
    movabsq $0x4002000000000000, %r9
    cmpq %r9, %r8
    jne fail_var_record
    movabsq $0x400c000000000000, %r9
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
    cmpq %r9, 8(%rsp)
    jne fail_var_record
    movl $23, (%rdx)
    movl $1, %eax
    ret
fail_var_record:
    xorl %eax, %eax
    ret
