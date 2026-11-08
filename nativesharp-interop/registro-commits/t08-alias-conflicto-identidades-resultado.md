# Identidad observada para `NMood`

Con el compilador incremental `1c722045a2aad1ed`, el consumidor con header
volvió a fallar en O0. Los alias tenían tipo canónico `enum NMood` y USR
`c:Api.h@T@NMood` iguales, pero el guard devolvió `reconciled=no`. En el
índice usado por esa resolución se observó solo el enum precompilado
`CNamesReferenced::NMood`, con USR `c:@E@NMood` y sin destino conciliado;
el enum vivo conciliado no estaba en esa lista.

La evidencia explica por qué el guard no encuentra el par, pero aún no
demuestra cómo transportar de forma segura la conciliación hasta esa fase.
El control termina con código 1 y la tanda 8 sigue abierta.
