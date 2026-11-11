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
.globl sysv_record_small
sysv_record_small:
    cmpl $1, %edi
    jne fail_record_small
    movabsq $0x0000000200000001, %r8
    cmpq %r8, %rsi
    jne fail_record_small
    movl $1, %eax
    ret
fail_record_small:
    xorl %eax, %eax
    ret
.globl sysv_record_mixed
sysv_record_mixed:
    cmpl $1, %edi
    jne fail_record_mixed
    cmpb $1, %al
    jb fail_record_mixed
    cmpl $7, %esi
    jne fail_record_mixed
    movq %xmm0, %r8
    movabsq $0x4002000000000000, %r9
    cmpq %r9, %r8
    jne fail_record_mixed
    movl $1, %eax
    ret
fail_record_mixed:
    xorl %eax, %eax
    ret
.globl sysv_record_two
sysv_record_two:
    cmpl $2, %edi
    jne fail_record_two
    cmpb $1, %al
    jb fail_record_two
    movabsq $0x0000000200000001, %r8
    cmpq %r8, %rsi
    jne fail_record_two
    cmpl $7, %edx
    jne fail_record_two
    movq %xmm0, %r8
    movabsq $0x4002000000000000, %r9
    cmpq %r9, %r8
    jne fail_record_two
    movl $1, %eax
    ret
fail_record_two:
    xorl %eax, %eax
    ret
.globl sysv_record_pressure
sysv_record_pressure:
    cmpl $6, %edi
    jne fail_record_pressure
    cmpl $1, %esi
    jne fail_record_pressure
    cmpl $2, %edx
    jne fail_record_pressure
    cmpl $3, %ecx
    jne fail_record_pressure
    cmpl $4, %r8d
    jne fail_record_pressure
    cmpl $5, %r9d
    jne fail_record_pressure
    movabsq $0x4002000000000000, %r10
    cmpq %r10, 8(%rsp)
    jne fail_record_pressure
    cmpl $7, 16(%rsp)
    jne fail_record_pressure
    movl $0, 16(%rsp)
    movl $1, %eax
    ret
fail_record_pressure:
    xorl %eax, %eax
    ret
.globl sysv_record_sse_pressure
sysv_record_sse_pressure:
    cmpl $9, %edi
    jne fail_record_sse_pressure
    cmpb $8, %al
    jne fail_record_sse_pressure
    movabsq $0x4002000000000000, %r10
    cmpq %r10, 8(%rsp)
    jne fail_record_sse_pressure
    cmpl $7, 16(%rsp)
    jne fail_record_sse_pressure
    movl $0, 16(%rsp)
    movl $1, %eax
    ret
fail_record_sse_pressure:
    xorl %eax, %eax
    ret
.globl sysv_record_large
sysv_record_large:
    cmpl $1, %edi
    jne fail_record_large
    cmpq $11, 8(%rsp)
    jne fail_record_large
    cmpq $13, 16(%rsp)
    jne fail_record_large
    cmpq $17, 24(%rsp)
    jne fail_record_large
    movq $0, 8(%rsp)
    movl $1, %eax
    ret
fail_record_large:
    xorl %eax, %eax
    ret
.globl sysv_record_ptrs
sysv_record_ptrs:
    testq %rdi, %rdi
    je fail_record_ptrs
    testq %rdx, %rdx
    je fail_record_ptrs
    cmpl $41, (%rdi)
    jne fail_record_ptrs
    cmpw $12, (%rdx)
    jne fail_record_ptrs
    cmpl $7, %esi
    jne fail_record_ptrs
    cmpb $1, %al
    jb fail_record_ptrs
    movq %xmm0, %r8
    movabsq $0x4002000000000000, %r9
    cmpq %r9, %r8
    jne fail_record_ptrs
    movl $42, (%rdi)
    movw $13, (%rdx)
    movl $1, %eax
    ret
fail_record_ptrs:
    xorl %eax, %eax
    ret
.globl sysv_record_return_ptr
sysv_record_return_ptr:
    movq %rdi, %r10
    callq sysv_record_ptrs
    testl %eax, %eax
    je fail_record_return_ptr
    movq %r10, %rax
    ret
fail_record_return_ptr:
    xorl %eax, %eax
    ret
.globl sysv_record_rptr
sysv_record_rptr:
    cmpl $2, %edi
    jne fail_record_rptr
    cmpl $7, %esi
    jne fail_record_rptr
    testq %rdx, %rdx
    je fail_record_rptr
    cmpl $1, (%rdx)
    jne fail_record_rptr
    cmpl $2, 4(%rdx)
    jne fail_record_rptr
    cmpb $1, %al
    jb fail_record_rptr
    movq %xmm0, %r8
    movabsq $0x4002000000000000, %r9
    cmpq %r9, %r8
    jne fail_record_rptr
    movl $3, (%rdx)
    movl $1, %eax
    ret
fail_record_rptr:
    xorl %eax, %eax
    ret
.globl sysv_record_fixed_rptr
sysv_record_fixed_rptr:
    testq %rdi, %rdi
    je fail_record_fixed_rptr
    testq %rdx, %rdx
    je fail_record_fixed_rptr
    cmpl $1, (%rdi)
    jne fail_record_fixed_rptr
    cmpl $2, 4(%rdi)
    jne fail_record_fixed_rptr
    cmpw $12, (%rdx)
    jne fail_record_fixed_rptr
    cmpl $7, %esi
    jne fail_record_fixed_rptr
    cmpb $1, %al
    jb fail_record_fixed_rptr
    movq %xmm0, %r8
    movabsq $0x4002000000000000, %r9
    cmpq %r9, %r8
    jne fail_record_fixed_rptr
    movl $3, (%rdi)
    movw $13, (%rdx)
    movq %rdi, %rax
    ret
fail_record_fixed_rptr:
    xorl %eax, %eax
    ret
.globl sysv_record_packed
sysv_record_packed:
    cmpl $1, %edi
    jne fail_record_packed
    cmpb $7, 8(%rsp)
    jne fail_record_packed
    movabsq $0x4002000000000000, %r10
    cmpq %r10, 9(%rsp)
    jne fail_record_packed
    movb $0, 8(%rsp)
    movl $1, %eax
    ret
fail_record_packed:
    xorl %eax, %eax
    ret
.globl sysv_record_packed_ptr
sysv_record_packed_ptr:
    cmpl $2, %edi
    jne fail_record_packed_ptr
    cmpl $7, %esi
    jne fail_record_packed_ptr
    testq %rdx, %rdx
    je fail_record_packed_ptr
    cmpb $7, (%rdx)
    jne fail_record_packed_ptr
    movabsq $0x4002000000000000, %r10
    cmpq %r10, 1(%rdx)
    jne fail_record_packed_ptr
    movq %xmm0, %r8
    cmpq %r10, %r8
    jne fail_record_packed_ptr
    cmpb $1, %al
    jb fail_record_packed_ptr
    movb $8, (%rdx)
    movl $1, %eax
    ret
fail_record_packed_ptr:
    xorl %eax, %eax
    ret
.globl sysv_record_array
sysv_record_array:
    cmpl $1, %edi
    jne fail_record_array
    cmpb $2, %al
    jb fail_record_array
    movq %xmm0, %r8
    movabsq $0x4002000000000000, %r9
    cmpq %r9, %r8
    jne fail_record_array
    movq %xmm1, %r8
    movabsq $0x400c000000000000, %r9
    cmpq %r9, %r8
    jne fail_record_array
    movl $1, %eax
    ret
fail_record_array:
    xorl %eax, %eax
    ret
.globl sysv_record_nested
sysv_record_nested:
    cmpl $1, %edi
    jne fail_record_nested
    cmpb $1, %al
    jb fail_record_nested
    movq %xmm0, %r8
    movabsq $0x4002000000000000, %r9
    cmpq %r9, %r8
    jne fail_record_nested
    movabsq $0x0000000200000001, %r9
    cmpq %r9, %rsi
    jne fail_record_nested
    movl $1, %eax
    ret
fail_record_nested:
    xorl %eax, %eax
    ret
