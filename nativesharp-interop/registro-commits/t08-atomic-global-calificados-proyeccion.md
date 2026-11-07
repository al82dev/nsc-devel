# T08: apuntados calificados en globales C atómicos no TLS

El control no TLS derivado del fixture existente rechazó los globales a
`const` y `volatile` short, long long y double como tipo no mapeado. Un control
separado en el mismo directorio reprodujo el rechazo para unsigned int const y
volatile. El predicado de `projectGlobalWithLookup` excluía estas variantes
solo por no ser TLS.

Se retira esa exclusión; se mantienen los tipos de apuntado, tamaños y
alineaciones previamente reconocidos. No se amplían otros tipos. Faltan build
incremental, controles positivos y negativos, IR y regresión TLS antes de
acreditar el comportamiento. No se modifica NSLink.
