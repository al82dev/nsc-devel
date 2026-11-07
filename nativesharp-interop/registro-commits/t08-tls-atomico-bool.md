# T08: lectura y escritura de TLS C `_Atomic(_Bool)`

NSC proyecta `_Thread_local _Atomic(_Bool)` cuando Clang informa almacenamiento
de ocho bits y alineación de al menos un byte. La lectura usa `load atomic i8`
`seq_cst` y convierte el resultado a `bool`; la escritura extiende el valor
booleano a un byte y usa `store atomic i8` `seq_cst`. Se preservan `const` y
`volatile`. El puntero atómico continúa rechazado por el control negativo.

La construcción incremental y `atomic/validar.cmd` pasaron en O0–O3. El control
C y NativeSharp comprueban valores iniciales, lecturas y escrituras normales y
`volatile`, lectura `const` y rechazo de escritura `const`. El IR se verifica
por símbolo, orden y alineación. Registros:
`A:/nsc-interop-build/tanda08-atomic-bool-build.log` y
`A:/nsc-interop-build/tanda08-atomic-bool-final.log`. Faltan otros usos y el
gate completo de T08.
