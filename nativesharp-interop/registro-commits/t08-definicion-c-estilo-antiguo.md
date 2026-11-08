# Tanda 8: llamada a definición C de estilo antiguo

El fixture de prototipos incluye una función con definición C11 de lista de
identificadores: sus parámetros declarados son `unsigned char` y `float`.
NativeSharp la llama a través de una declaración sin prototipo con argumentos
`byte` y `float`, y comprueba el resultado. El fixture completo pasó O0–O3 y
conservó los controles de rechazo de aridad. Clang emitió seis advertencias
de declaraciones o definiciones sin prototipo obsoletas.

Evidencia en `A:\nsc-interop-build\tanda08-prototipos-c`. Es un caso dirigido
para el objetivo Windows x64; no acredita todas las convenciones antiguas ni
cierra T08-003.
