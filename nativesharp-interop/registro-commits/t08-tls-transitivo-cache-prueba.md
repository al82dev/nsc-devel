# Prueba de caché con cambio a TLS transitivo

El fixture reconstruye el mismo productor con una dependencia C que usa TLS y
vuelve a construir dos veces el consumidor previamente generado sin TLS. Se
comprueban selección LLD, ejecución y directorio TLS en ambos resultados. La
prueba no da por demostrado un acierto de caché; comprueba la corrección con la
caché habilitada y deja los registros para inspeccionar el comportamiento.
