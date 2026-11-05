.text
.globl foo
.globl __imp_foo
foo:
  jmp *__imp_foo(%rip)
.section .idata$2
.globl __IMPORT_DESCRIPTOR_foo
__IMPORT_DESCRIPTOR_foo:
  .rva .Llookup
  .long 0
  .long 0
  .rva .Ldll
  .rva __imp_foo
.section .idata$3
.globl __NULL_IMPORT_DESCRIPTOR
__NULL_IMPORT_DESCRIPTOR:
  .zero 20
.section .idata$4
.p2align 3
.Llookup:
  .rva .Lhint_name
  .long 0
  .quad 0
.section .idata$5
.p2align 3
__imp_foo:
  .rva .Lhint_name
  .long 0
  .quad 0
.section .idata$6
.Ldll:
  .asciz "foo.dll"
.p2align 1
.Lhint_name:
  .short 0
  .asciz "foo"
