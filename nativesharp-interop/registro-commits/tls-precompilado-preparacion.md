# Matriz de TLS C con productor NativeSharp precompilado

Se preparó un productor estático NativeSharp que lee y escribe un global TLS
C y comprueba su aislamiento entre hilos. Un consumidor comparte el header C
y comprueba que observa el mismo estado; otro utiliza solo la API publicada
por el productor y se compila con la copia de `Api.h` oculta temporalmente.
La fuente original permanece intacta y la copia se restaura al terminar la
compilación incluso si NSC falla. El script incluye control C y O0–O3.

La reproducción inicial aislada del primer consumidor pasó O0 con NSC build
`b151b46f0be04227`. La matriz completa y el caso sin header todavía no han
sido validados. NSLink no se modifica; se usa LLD explícitamente desde A:.
