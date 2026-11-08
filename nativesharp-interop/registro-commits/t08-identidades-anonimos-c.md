# Identidades internas de agregados anónimos C11

La función `materializeCppAnonymousCRecords` convierte una observación C completa
en una copia del propietario y declaraciones anidadas con identidades internas
deterministas según la ruta de carriers. Verifica rutas duplicadas o huérfanas,
dimensiones, tipo del carrier y colisiones con nombres de campos fuente. No muta
la observación Clang original. La prueba prevista comprueba dos niveles y colisión.

La función todavía no instala los tipos ni publica metadata: el consumo directo y
precompilado de estos miembros sigue pendiente. Las nuevas pruebas no se han
ejecutado, y la tanda 8 continúa abierta.
