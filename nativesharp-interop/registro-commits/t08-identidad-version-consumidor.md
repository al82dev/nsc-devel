# T08: restaurar la versión del catálogo en el control de identidad

El consumidor construido por la prueba de restauración copiaba el catálogo real,
pero no su versión de NSMETA. Con el catálogo de versión 7, lo interpretaba
con la versión predeterminada y fallaba antes de comprobar los USR.

El módulo de prueba recibe el minor del productor, igual que la ruta del driver.
La validación dirigida queda pendiente de la construcción incremental agrupada.
