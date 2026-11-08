# Global C atómica compartida entre dos bibliotecas

Dos productores precompilados leen y actualizan la misma global externa
`_Atomic(int)`. El consumidor ejecutó correctamente el caso en O0. En el IR
generado se comprobaron una lectura `load atomic` y una actualización
`atomicrmw add`, ambas `seq_cst`. La variante que mezcla `_Atomic(int)` con
`int` fue rechazada por ambigüedad.

Los controles se reproducen con `externa-dos-bibliotecas/validar.cmd` y los
argumentos `atomic-compatible` y `atomic`. No demuestran concurrencia ni
cubren otros tipos atómicos, optimizaciones o modos de enlace.
