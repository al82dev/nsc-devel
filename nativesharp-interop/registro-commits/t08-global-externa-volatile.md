# Conciliación de una global C externa volatile

Dos productores precompilados declaran la misma global `volatile int` externa;
el consumidor lee y modifica el estado compartido. La prueba compiló y ejecutó
correctamente en O0. Una variante con `int` en un productor y `volatile int` en
el otro fue rechazada por ambigüedad. La variante de tipos `int` y `long` también
se rechazó, y la variante sin calificadores siguió pasando.

Los casos se reproducen con `externa-dos-bibliotecas/validar.cmd`, argumentos
`volatile-compatible` y `volatile`. No se verificaron aquí otros calificadores,
optimizaciones ni modos de enlace.
