# T08 H3: expectativas de la matriz de identidad

La matriz de plantillas esperaba errores para casos que ya funcionan con el catálogo de dependencias C 1.46. Se actualizan las filas de función/global, encabezado, alias, escritura de global y `static inline` para exigir compilación y ejecución. La fila de record local conserva el rechazo observado; esta actualización no modifica su expectativa.

`pruebas-tanda-08-identidad-c/validar.cmd` terminó con código 0. El registro `A:/nsc-interop-build/tanda08-identidad-actual-2.log` muestra las cinco filas compiladas y ejecutadas, así como el rechazo todavía esperado de `ConsumerRecordLocal`. Este validador incluye controles negativos y su resultado no implica el cierre de T08-002 ni de la tanda 8.
