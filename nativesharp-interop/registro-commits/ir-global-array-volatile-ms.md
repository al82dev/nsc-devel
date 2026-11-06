# Discrepancia de orden en arreglo global C volatile

## Evidencia observada

El IR previo de NSC en `A:/nsc-interop-build/tanda08-record-c/project.ll`
contiene para `volatile_array[1]` una escritura `store volatile i32` y una
lectura `load volatile i32`, ambas con alineación 4 y sin orden atómico.

Con Clang de `A:/al82dev-clang/bin` se compiló el `Api.c` actual para
`x86_64-pc-windows-msvc`, C11, O0 y salida LLVM IR, en
`A:/nsc-interop-build/tanda08-record-c-volatile-reference.ll`. La función C
`read_volatile_arrays` lee el mismo global mediante `load atomic volatile i32`
con orden `acquire` y alineación 4. La escritura NS previa no tiene en esta
comparación una escritura C equivalente y por sí sola no demuestra la
semántica que Clang utilizaría para una escritura.

## Consecuencia y límites

El caso concreto de lectura no conserva la misma semántica de orden que la
referencia Clang. Una prueba que solo busque la palabra `volatile` no detecta
esta diferencia. El IR NS corresponde a una ejecución anterior; se debe
regenerar junto con las modificaciones preparadas antes de afirmar el estado
del binario más reciente. No se infiere que todos los campos, apuntados o
plataformas utilicen el mismo orden.

En el código actual, `HirCompletionAccessFrames.cpp` propaga el modo de
volatile en arreglos desde direcciones de campos y elementos, pero la condición
no incluye directamente `NamespaceStateAddress`. El helper de lowering
`clangMsVolatileEnumElementAlignment` exige información y alineación que deben
examinarse para este caso; observar estas rutas no constituye todavía una
causa completa ni autoriza generalizar una corrección. No se modificó NSLink.
