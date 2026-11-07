# Matriz de TLS C con productor NativeSharp precompilado

Se preparó un productor estático NativeSharp que lee y escribe un global TLS
C y comprueba su aislamiento entre hilos. Un consumidor comparte el header C
y comprueba que observa el mismo estado; otro utiliza solo la API publicada
por el productor y se compila con la copia de `Api.h` oculta temporalmente.
La fuente original permanece intacta y la copia se restaura al terminar la
compilación incluso si NSC falla. El script incluye control C y O0–O3.

La reproducción inicial aislada del primer consumidor pasó O0 con NSC build
`b151b46f0be04227`. La ejecución posterior de `precompiled/validar.cmd` pasó
el control C y las dos variantes del consumidor en O0–O3 con el mismo build.
En cada nivel se ocultó la copia utilizada por el productor mientras se
compilaba el consumidor sin header y se restauró después. `fc /b` confirmó
que la copia final coincide con el header fuente y no quedó `Api.h.hidden`.
El header fuente siguió disponible; no se afirma aislamiento de todo el
sistema de archivos. NSLink no se modifica; se usa LLD explícitamente desde A:.
Esto no cierra T08-004 ni la tanda 8.
