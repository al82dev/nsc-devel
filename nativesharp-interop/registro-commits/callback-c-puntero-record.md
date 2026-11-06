# Callback C con puntero a record

El fixture prepara una firma C `int (*)(CallbackPayload*)`, retornada por una
función y almacenada en un campo y un global. NativeSharp crea el record y
llama el callback desde las tres rutas; la implementación C lee su campo.
Esta primera preparación no demuestra soporte. Se investigará el diagnóstico
antes de cambiar producción y se validará la solución junto a la tanda.

## Reproducción y causa

Con NSC build `bd1f1ac94128ecb4` y el proyecto compilado por
`--dump-project-oop-llvm`, el compilador reportó que el receptor
`RecordCallbackHolder` no tiene un miembro compatible `run` y que el global
`record_callback_global` tiene un tipo C/C++ todavía no mapeado. No reportó
error en la línea que obtiene y llama el callback retornado; al fallar el
proyecto no se llegó a generar ni ejecutar su IR completo.

En `CppSemanticRecord.cpp` y `CppSemanticInventory.cpp`, la proyección de
callbacks almacenados llama `projectCppStoredFunctionPointer(field.type)` o
su equivalente para globales. Esta función invoca `mapFunctionType` sin
`ProjectNameLookup`, propietario ni módulo, y la rama de record exige un
índice de records. Así se pierde la resolución necesaria para
`CallbackPayload*`. La ruta de retorno de funciones usa otro contexto.

## Cambio preparado

El mapeador de callbacks almacenados recibe el proyecto, módulo y lookup de
tipos ya disponibles en los dos consumidores. Para un puntero directo a
record dentro de la firma, intenta usar un tipo C importado sin conflicto,
verifica su propietario o reconciliación y rechaza identidades USR opuestas.
La ruta es específica de callbacks almacenados; los parámetros o resultados
record por valor y las formas sin información de identidad siguen rechazados.
El fixture se incorporó a la matriz O0–O3. El cambio está preparado y aún no
se ha construido ni validado; no cierra T08-017.

Se prepararon además controles de sesión Clang para el mismo mapeador con
lookup real: identidad válida, ausencia de contexto, propietario incorrecto y
USR diferente. Son pruebas pendientes de construcción y ejecución.

Para aislar la proyección del almacenamiento de la resolución de una llamada
escrita directamente sobre un campo o global, el fixture lee cada callback
en una variable local antes de invocarlo. Es la forma ya utilizada por el
fixture escalar `Stored.ns`. Posteriormente se agregaron también las llamadas
directas al campo y global; ambas esperan validación con el cambio preparado.
Con el compilador anterior, esta variante también se rechazó: `holder->run`
no encuentra el campo compatible y `record_callback_global` conserva el
diagnóstico de tipo no mapeado. Por ello el fallo reproducido no depende de la
forma de invocación directa.
