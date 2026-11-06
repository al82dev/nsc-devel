# Conversión de enum C a entero y a otro enum C

Se prepara una prueba real con conversión a int, conversión entre enums C y
extensión de un valor unsigned a 64 bits. Incluye un control de que el enum
NativeSharp conserve sus reglas. Pendiente reproducir y diagnosticar.
Se añade una extensión de enum signed además de la unsigned, y Clang comprueba
ambas bases en C mediante `_Static_assert` antes de la ejecución.

Pasó O0–O3 con `2d0b3a78ed4e6747`, incluido el control negativo NativeSharp.
El IR confirmó extensiones signed y unsigned distintas; ver el registro
`conversion-enum-c-entero.md` para los límites.
