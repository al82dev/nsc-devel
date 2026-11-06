# Dependencia C compartida por productores precompilados

Dos bibliotecas NativeSharp precompiladas que declararon el mismo `api.lib`
produjeron NSL1021 para `external_double` al enlazar su consumidor. Las
bibliotecas de los productores no contienen `api.lib`: la dependencia se
resuelve desde su metadata y `validateLibraryReference` la agregaba una vez
por cada productor sin consultar las rutas ya añadidas.

El ajuste evita volver a agregar la misma ruta ya suministrada por un productor
anterior o por el proyecto. Conserva el orden y las repeticiones dentro de cada
lista individual y mantiene la verificación de identidad de las dependencias.
NSLink no se modifica. Se restituye la dependencia en el segundo productor para
ejercitar el caso original; queda pendiente la construcción y validación.
