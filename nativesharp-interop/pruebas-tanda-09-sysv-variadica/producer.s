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
.globl sysv_ptrs
sysv_ptrs:
    cmpl $3, %edi
    jne fail_ptrs
    testq %rsi, %rsi
    je fail_ptrs
    testq %rdx, %rdx
    je fail_ptrs
    testq %rcx, %rcx
    je fail_ptrs
    cmpw $-1234, (%rsi)
    jne fail_ptrs
    cmpl $42, (%rdx)
    jne fail_ptrs
    cmpb $-12, (%rcx)
    jne fail_ptrs
    movw $1234, (%rsi)
    movl $43, (%rdx)
    movb $12, (%rcx)
    movl $1, %eax
    ret
fail_ptrs:
    xorl %eax, %eax
    ret
.globl sysv_empty
sysv_empty:
    testl %edi, %edi
    jne fail_empty
    testb %al, %al
    jne fail_empty
    movl $1, %eax
    ret
fail_empty:
    xorl %eax, %eax
    ret
.globl sysv_floatspill
sysv_floatspill:
    cmpl $9, %edi
    jne fail_floatspill
    cmpb $8, %al
    jne fail_floatspill
    movabsq $0x4002000000000000, %r9
    movq %xmm0, %r8
    cmpq %r9, %r8
    jne fail_floatspill
    movq %xmm7, %r8
    cmpq %r9, %r8
    jne fail_floatspill
    movabsq $0x400c000000000000, %r9
    cmpq %r9, 8(%rsp)
    jne fail_floatspill
    movl $1, %eax
    ret
fail_floatspill:
    xorl %eax, %eax
    ret
.globl sysv_fixed_ptrs
sysv_fixed_ptrs:
    testq %rdi, %rdi
    je fail_fixed_ptrs
    testq %rsi, %rsi
    je fail_fixed_ptrs
    cmpl $42, (%rdi)
    jne fail_fixed_ptrs
    cmpw $-1234, (%rsi)
    jne fail_fixed_ptrs
    cmpl $-7, %edx
    jne fail_fixed_ptrs
    movq %xmm0, %r8
    movabsq $0x4002000000000000, %r9
    cmpq %r9, %r8
    jne fail_fixed_ptrs
    cmpb $1, %al
    jb fail_fixed_ptrs
    movl $43, (%rdi)
    movw $1234, (%rsi)
    movl $1, %eax
    ret
fail_fixed_ptrs:
    xorl %eax, %eax
    ret
.globl sysv_return_ptr
sysv_return_ptr:
    testq %rdi, %rdi
    je fail_return_ptr
    cmpl $7, %esi
    jne fail_return_ptr
    movl $44, (%rdi)
    movq %rdi, %rax
    ret
fail_return_ptr:
    xorl %eax, %eax
    ret
