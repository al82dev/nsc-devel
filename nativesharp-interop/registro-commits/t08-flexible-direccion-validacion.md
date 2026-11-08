# T08: validación de dirección flexible subalineada

Con los cambios hasta `1448b7a5e`, la compilación incremental de `nsc` terminó
con código 0. El control `pruebas-tanda-08-flexible-c/validar.cmd` terminó con
código 0: comprobó la nueva restricción, lectura/escritura directa y
precompilada O0–O3, e IR de las muestras incluidas.

La sonda de conversión implícita sigue rechazada y es informativa en ese
validador. El origen del miembro se transporta al binding, pero aún no tiene
consumidor ni prueba específica. No se cierra la tanda 8.
