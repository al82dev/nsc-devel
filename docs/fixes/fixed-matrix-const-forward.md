# Dimensiones const reenviadas a matrices fijas

El ejemplo 70 de matrices fijas rechazaba `Rows` como argumento que debía ser
un valor constante, aun cuando era un parámetro `const` del método genérico.
La reproducción copiada se conservó en `A:/nsc-fixes-evidencia/cases/70-static`.
Antes del cambio el diagnóstico está registrado en `baseline-fixed-matrix.log`.

El planificador valida primero la cantidad de argumentos. Para una referencia
abierta conserva el parámetro exterior cuando su clase (tipo o constante)
coincide con la del parámetro de destino; valida de inmediato los argumentos
concretos. El ámbito del método registra cuáles parámetros son constantes.
La especialización cerrada sigue el camino de normalización existente.

`nsc.compiler.fixed-matrix-const-forward` compila y ejecuta una multiplicación
2x2 con dimensiones reenviadas en O0 y O2; comprueba también que se rechacen
un tipo concreto y un parámetro de tipo usados como valores constantes.
Pasaron además `nsc.stage44.llvm` y `nsc.cc1.generic-constraints`; registros
`matrix-final.log`, `matrix-stage44.llvm.log` y
`matrix-generic-constraints.log` en `A:/nsc-fixes-evidencia`.

La prueba cubre el método genérico del ejemplo y sus argumentos inválidos,
no todas las combinaciones de ámbitos, alias o paquetes genéricos. No cambia
la decisión pendiente sobre métodos genéricos en tipos genéricos.
