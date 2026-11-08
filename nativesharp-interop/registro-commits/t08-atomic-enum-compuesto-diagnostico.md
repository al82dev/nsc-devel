# Tanda 8: operaciones compuestas sobre enum C atómico

Una sonda C11 compilada con Clang 23.1.0 para Windows x64 aceptó `state += 1`,
`state++` y `state |= 1` cuando `state` es `_Atomic(enum State)`. La misma
secuencia en NativeSharp se rechazó durante análisis semántico: las asignaciones
compuestas exigen operandos numéricos y el incremento exige un tipo numérico o
puntero. Las sondas utilizadas están en
`A:\nsc-interop-build\atomic-enum-compound-probe`.

La corrección requiere coordinar el análisis de asignaciones e incrementos con
el lowering de lectura-modificación-escritura de enum; no basta convertir el
enum a entero y hacer una carga y una escritura separadas, porque se perdería
la atomicidad. Es necesario conservar las promociones, conversiones y valores
de prefijo y sufijo, y contrastar el IR y el comportamiento con Clang. Esta
nota es un diagnóstico: no acredita implementación ni cierra la tanda 8.
