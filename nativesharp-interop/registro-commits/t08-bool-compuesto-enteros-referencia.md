# T08-015: referencia de `_Bool` C compuesto con enteros anchos

Clang 23.1.0 compiló las funciones `xor_uint`, `xor_long` y `xor_ulong` de
`BoolCompoundReference.c` con destino Windows x64 MSVC, C11 y O0. En los tres
casos lee el byte volatile acquire y normaliza tras operar; con `unsigned int`
usa i32 y con `long long` y `unsigned long long` usa i64. La escritura es byte
volatile release. IR: `A:/nsc-interop-build/t08-bool-compuesto-wide-ref.ll`.

Esta referencia no valida NSC. Las operaciones de división necesitan conservar
también el signo apropiado; los desplazamientos se tratan por separado porque
su conversión difiere de la conversión aritmética usual.
