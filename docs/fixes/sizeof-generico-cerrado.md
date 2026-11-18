# `sizeof` y `alignof` de un tipo genérico cerrado (LKF-079)

`sizeof(Box<int>)` fallaba con "expected expression" en O0 y O2: `looksLikeSizeofTypeForm` decidía si
el operando es un tipo mirando el nombre, sus puntos, punteros y corchetes, y se detenía en `<`, así
que el operando se analizaba como una comparación.

La comprobación acepta ahora, después del nombre, una lista de argumentos balanceada que solo
contiene lo que un argumento de tipo puede escribir: nombres, tipos predefinidos, puntos, comas,
punteros, corchetes y enteros para parámetros `const`. `>>` cierra dos niveles. Una comparación como
`sizeof(a < b)` no coincide y sigue siendo una expresión.

`tests/integration/sizeof_closed_generic.py` (`nsc.compiler.sizeof-closed-generic`): `Box<int>` (8,
alineación 4), `Box<long>` (16, 8), `Box<byte>` (8), `Box<Box<long>>` (24, 8), la forma de expresión
`sizeof(local)` y el uso del valor, salida 42 en O0 y O2. Un argumento desconocido informa "unknown
type name 'Missing'" en su posición. Pasan también las pruebas `sizeof`, `alignof`, cc1 y del parser.
