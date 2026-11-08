# Conciliación de una global C externa const

Dos productores precompilados leen una misma global C `const int`; el
consumidor enlazó y ejecutó correctamente en O0. La variante con `const int`
en un productor e `int` en el otro fue rechazada por ambigüedad.

Los casos se reproducen con `externa-dos-bibliotecas/validar.cmd`, argumentos
`const-compatible` y `const`. No se verificaron aquí otros calificadores,
optimizaciones ni modos de enlace.
