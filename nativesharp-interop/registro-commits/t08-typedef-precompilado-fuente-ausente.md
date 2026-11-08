# Typedef C precompilado sin fuente original

Los controles equivalentes de record y enum se rechazaban si el encabezado del
productor ya no estaba disponible. La comprobación compara ahora el contrato C
publicado con el encabezado presente; cuando la fuente original falta, exige que
la otra fuente esté disponible y coincida con su huella. Si una fuente presente
fue modificada, el caso sigue rechazándose. No se cambia el formato NSMETA.

Los validadores `typedefs/precompiled-conflict/validar.cmd` y
`typedefs/precompiled-enum/validar.cmd` construyeron en O0 el productor y
consumidor, retiraron una copia temporal del encabezado original y ejecutaron
los controles equivalentes. También verificaron rechazo de incompatibilidades y
de una fuente modificada. El record incluye lectura y escritura; el enum solo
ejercita un puntero, por lo que operaciones sobre valores siguen pendientes.
La construcción incremental de NSC en A: terminó con dos advertencias existentes
de inicializadores. No se ejecutó la batería completa de la tanda.
