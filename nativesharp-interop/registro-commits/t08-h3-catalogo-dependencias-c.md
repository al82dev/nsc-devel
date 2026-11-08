# T08 H3: catalogo interno de dependencias C

Se prepara una envoltura interna que reune instantaneas de funciones, globales y aliases. Ordena las entradas por su representacion codificada, rechaza duplicados exactos y valida version, limites, truncamiento y codificacion canonica al leer. No modifica NSMETA ni decide si dos declaraciones distintas representan la misma entidad; esa conciliacion sigue pendiente.

Se agregaron controles de ida y vuelta de las tres categorias, duplicados y truncamiento. No se compilaron ni ejecutaron aun: se agruparan con los siguientes cambios. Faltan captura con ambitos y sombras, comprobacion de conflictos semanticos, normalizacion de rutas, versionado publicado, restauracion y pruebas de extremo a extremo.
