# Caché de interoperabilidad con ruta larga

El paquete con dos DLL TLS homónimas se compilaba pero no se publicaba en una
raíz de caché larga: la segunda compilación reportaba `not-found` en lugar de
`hit`. El nombre de la entrada SHA-256 y los nombres de los artefactos hacían
que la ruta de preparación superara el límite convencional de Windows.

`CompilationBundleCache::setRoot` usa ahora el formato extendido de Windows para
raíces absolutas, incluida la forma UNC. No cambia la estructura lógica de la
caché ni las rutas relativas. La prueba dirigida compila, restaura desde caché
y ejecuta un consumidor con dos DLL en O0–O3 usando la ruta que fallaba. Se
integró al validador de la tanda. La compilación de NSC fue incremental.

Las referencias directas a encabezados C/C++ siguen sin caché: todavía falta
identificar de forma segura todas las entradas transitivas. La tanda 8 permanece
abierta y no se ha ejecutado su validación completa en este cambio.
