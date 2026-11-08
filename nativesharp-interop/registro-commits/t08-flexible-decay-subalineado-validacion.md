# T08: validación de conversión flexible subalineada

Con `5fb3d5c61`, la construcción incremental de `nsc` pasó y el validador
`validar-decay.cmd` terminó con código 0. Los casos directos y precompilados
pasaron O0–O3, y se rechazaron la dirección explícita y la conversión implícita
del elemento `int` empaquetado.

Esta evidencia no cubre otros elementos empaquetados ni los calificadores
const/volatile. La tanda 8 sigue abierta.
