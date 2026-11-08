# Comparación de agregados anónimos C11

La selección de definiciones repetidas de un record compara ahora los árboles de
agregados anónimos capturados: rutas de carriers, clases, dimensiones y campos,
incluyendo sus offsets, tipos e identidades conocidas. También compara la marca
de carrier de los campos exteriores. Así no se seleccionan como equivalentes dos
definiciones cuyo contenido anónimo sea distinto. Una prueba de selección contrasta
dos observaciones iguales y otra con el nombre de un campo anónimo distinto.

La construcción y las pruebas agrupadas siguen pendientes. Esto no habilita aún
la proyección ni la publicación del árbol; T08-010 y T08-011 permanecen abiertos.
