# Orden de acceso mediante volatile int* C

## Evidencia

La referencia Clang 23 en
`A:/nsc-interop-build/tanda08-record-c-volatile-reference.ll` contiene la
función `bump_volatile`, que recibe `volatile int*` y emite carga
`atomic volatile` con `acquire` y escritura `atomic volatile` con `release`,
ambas alineadas a 4. `read_volatile_pointer_array` también lee el objeto
apuntado con `acquire`.

El IR NS previo en `A:/nsc-interop-build/tanda08-record-c/project.ll` muestra
`store volatile i32 61` mediante `volatile_pointee`, sin `release`. Las
comparaciones corresponden a accesos de 32 bits en Windows x64; el IR NS debe
regenerarse con el compilador actual antes de afirmar el resultado vigente.

## Ruta y limitación

`ExpressionResult` y `MethodExpressionTypeBinding` conservan
`pointerPointeeVolatile`, pero no el modo volatile de Clang. La desreferencia
en `HirCompletionTerminalFrames.cpp` transmite esa marca al nodo, sin el modo
ni una garantía específica de alineación del objeto apuntado. No es seguro
aplicar el orden MS a todos los punteros volatile de NativeSharp o a una
dirección sin alineación comprobada. Hay que seguir la procedencia por
parámetros, resultados, globales, indexación y locales antes de definir la
corrección; este registro no la declara implementada ni probada.
