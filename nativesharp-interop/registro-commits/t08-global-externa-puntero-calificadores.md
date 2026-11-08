# Identidad de puntero C externo compartido

Dos productores precompilados utilizan el mismo puntero C externo en plantillas;
el consumidor lee el valor apuntado. La declaración `int *` coincidente compiló
y ejecutó correctamente en O0. Las variantes que declaran el objeto apuntado
como `const int` o `volatile int` en un solo productor fueron rechazadas por
ambigüedad, sin unir las declaraciones incompatibles.

Los controles se reproducen con `puntero-externo-dos-bibliotecas/validar.cmd`
y los argumentos `compatible`, `const` y `volatile`. No cubren todos los niveles
de punteros, otros calificadores, optimizaciones ni modos de enlace.
