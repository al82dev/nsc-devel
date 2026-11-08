# Selección de tipos internos anónimos C

La prueba `ProbeAnonymous` rechazó la dependencia del primer tipo interno: el
selector difería todos los tipos anidados y el tipo sintetizado carecía de ubicación.
Los records generados para agregados anónimos C llevan ahora una marca interna y
la ubicación de su propietario. El selector solo exceptúa del diferimiento a un
grupo compuesto exclusivamente por esos records C internos.

La corrección aún no se ha construido ni ejecutado; el caso directo, el
precompilado y la validación completa de la tanda 8 permanecen pendientes.
