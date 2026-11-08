# Tanda 8: reproducción de alias C en plantilla pública

Se agregó una prueba con un `.nsproj` que contiene una referencia C real y una
plantilla pública cuya declaración usa el typedef `CIdInt` del encabezado.
La prueba invoca directamente el proveedor con el formato 46 preparado y exige
que el catálogo contenga el alias.

La compilación incremental del ejecutable pasó. La ejecución falló: hay un
candidato de alias, pero el nodo original de la plantilla no tiene binding
(`binding=0`, `selected=0`), por lo que el productor devuelve un catálogo
vacío. La prueba queda roja como regresión hasta resolver esa ruta. No se ha
probado todavía el consumo ni la emisión normal de NSMETA 46.
