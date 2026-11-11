.text
.globl t09_var_record
t09_var_record:
    cmpl $1, %edi
    jne fail_var_record
    cmpb $0, %al
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
