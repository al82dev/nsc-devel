# T08: lectura y escritura de TLS C `_Atomic(int*)`

Se proyecta el global C TLS cuyo almacenamiento atómico contiene `int*` sin
calificadores en el tipo apuntado, con tamaño de 64 bits y alineación mínima de
8 bytes. El descenso usa cargas y escrituras `seq_cst` de 64 bits y convierte
entre representación entera y puntero; preserva `const` y `volatile` del
almacenamiento. El tipo apuntado calificado, la aritmética compuesta y tomar la
dirección del almacenamiento se rechazan explícitamente y siguen pendientes.

La construcción incremental y `atomic/validar.cmd` pasaron en O0–O3. Los
controles C y NativeSharp comprueban nulo, escritura, lectura, desreferencia,
`volatile`, lectura `const` y rechazo de escritura `const`; el IR se verifica
por símbolo, orden y alineación. También se verifican los rechazos pendientes.
Registros: `A:/nsc-interop-build/tanda08-atomic-pointer-guard-build.log` y
`A:/nsc-interop-build/tanda08-atomic-pointer-qualified.log`. No se ha cerrado
T08 ni validado toda la familia de punteros.

Actualización posterior: la aritmética del caso `int*` se implementó y se
documenta en `t08-tls-atomico-puntero-aritmetica.md`; las demás limitaciones
de esta entrada permanecen pendientes.
