# T08-002: restauración de USR por la ruta del driver

Nuevo modo `--verify-driver-restored-c-usr <biblioteca> <encabezado> <recursos>
<clang>`. Escribe un consumidor con un `LibraryReference` real a la biblioteca del
productor, carga su grafo, deja que `driver::configurePrecompiledModules`
prepare los módulos precompilados (carga de la biblioteca, transporte del
catálogo e instalación de proveedores) y compila los miembros. Después compara
con Clang el USR de cada record, campo, enum y enumerador restaurado. El
ejecutable enlaza `nsc_driver` (`CMakeLists.txt`).

El enum vivo se localiza por la lista de enumeradores, porque un enum anónimo
nombrado por typedef puede tener nombre vacío en Clang.

Nuevo fixture `pruebas-tanda-08-identidad-c/anonimos` con `Anon.h`, un productor
para un struct nombrado solo por typedef (`ProducerAnonRecord.ns`) y otro para un
enum anónimo nombrado por typedef (`ProducerAnonKind.ns`). El validador ejecuta
ambos modos para los cinco productores en O0–O3. `ProducerAnonLevel.ns` publica
un record con un campo de enum anónimo y solo se observa: el productor falla con
`C enum field dependency has no importable identity` (hallazgo H4).
