.text
.globl t09_var_record
t09_var_record:
    cmpl $1, %edi
    jne fail_var_record
    cmpl $7, %esi
    jne fail_var_record
    cmpb $1, %al
    jne fail_var_record
    movq %xmm0, %r8
    movabsq $0x4002000000000000, %r9
    cmpq %r9, %r8
    jne fail_var_record
    cmpl $1, 8(%rsp)
    jne fail_var_record
    cmpl $2, 12(%rsp)
    jne fail_var_record
    cmpl $3, 16(%rsp)
    jne fail_var_record
    cmpl $4, 20(%rsp)
    jne fail_var_record
    cmpl $5, 24(%rsp)
    jne fail_var_record
    movl $99, 8(%rsp)
    movl $1, %eax
    ret
fail_var_record:
    xorl %eax, %eax
    ret
