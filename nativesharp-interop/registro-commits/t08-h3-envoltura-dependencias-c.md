# T08 H3: envoltura interna de tipos y dependencias C

Se prepara una envoltura interna versionada que combina el catalogo existente de records y enums con las instantaneas de funciones, globales y aliases. La lectura comprueba tamaños, truncamiento y representacion canonica, y conserva las salidas previas ante errores. Se exige al menos una dependencia para no cambiar la representacion de bibliotecas que no las necesitan.

El valor interno de version no habilita ningun formato publicado. Antes de usarlo en NSMETA faltan la seleccion por version del archivo, verificacion de propietario y destino, conciliacion semantica, normalizacion de rutas, proyeccion ABI, publicacion y restauracion. Se prepararon controles de ida y vuelta y truncamiento; construccion y ejecucion pendientes para la validacion agrupada.
