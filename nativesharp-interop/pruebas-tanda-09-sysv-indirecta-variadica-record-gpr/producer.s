.text
.globl t09_var_record
t09_var_record:
    cmpl $1, %edi
    jne fail_var_record
    cmpl $2, %esi
    jne fail_var_record
    cmpl $3, %edx
    jne fail_var_record
    cmpl $4, %ecx
    jne fail_var_record
    cmpl $5, %r8d
    jne fail_var_record
    cmpl $6, %r9d
    jne fail_var_record
    cmpb $0, %al
    jne fail_var_record
    movq 8(%rsp), %r10
    movabsq $0x4002000000000000, %r11
    cmpq %r11, %r10
    jne fail_var_record
    cmpl $7, 16(%rsp)
    jne fail_var_record
    movq 24(%rsp), %r10
    testq %r10, %r10
    je fail_var_record
    cmpl $13, (%r10)
    jne fail_var_record
    movl $99, 16(%rsp)
    movl $23, (%r10)
    movl $1, %eax
    ret
fail_var_record:
    xorl %eax, %eax
    ret
