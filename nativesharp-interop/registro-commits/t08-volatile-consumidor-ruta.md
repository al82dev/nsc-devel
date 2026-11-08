# T08-010/015: ruta del modo volatile del consumidor

## Estado observado

- `CppLibraryReference.VolatileMode` pertenece a cada encabezado; el parser lo
  procesa en `src/Compilation/ProjectLoader.cpp` y cada sesión lo resuelve por
  separado. No hay una propiedad equivalente en el `Project` consumidor.
- `src/Interop/Cpp/CppSemanticRecord.cpp` fusiona el modo de observaciones de
  un mismo record: si difieren, conserva `nullopt`. Esa información viaja en
  el campo proyectado y en el catálogo, pero no define por sí sola el modo de
  acceso del código consumidor.
- `HirCompletionAccessFrames.cpp` hereda el modo desde ciertas direcciones de
  campos o globales. Para un alias local ya no dispone de ese origen; el IR
  pierde acquire/release en el modo Microsoft, incluso con reasignación.
- El contrato NativeSharp de concurrencia mantiene `volatile` separado de
  sincronización. Cambiar todos los punteros NativeSharp a acquire/release al
  importar un encabezado sería un cambio del comportamiento nativo.

## Dirección de implementación

La solución debe separar el modo de las sesiones de Clang del contrato de
acceso de la unidad que se compila. Ese contrato debe tener una selección
inequívoca para proyectos con referencias Microsoft e ISO, y debe mantenerse
por proyecto al compilar el grafo; no se puede elegir una referencia por su
orden. Debe distinguir además el acceso de interoperabilidad al almacenamiento
C del acceso a almacenamiento nativo de NativeSharp.

La ruta a completar comprende el descriptor y `Project`, su transmisión desde
`ProjectCompilation.cpp` al modelo HIR y la identificación de cada acceso por
puntero en `HirCompletionAccessFrames.cpp` y `HirCompletionTerminalFrames.cpp`.
El modo y la información necesaria para reconocer el acceso deben conservarse
en el análisis de expresiones, a través de parámetros, asignaciones, ramas y
consumo precompilado. `HirCacheIdentity.cpp` y `MirCache.cpp` deben distinguir
cualquier cambio que afecte al código generado. La representación concreta de
esa información todavía no está implementada ni probada.

La validación debe comparar Clang Microsoft e ISO en O0–O3, acceso directo,
alias y reasignaciones, llamadas entre funciones y proyectos, referencias
mixtas, metadata, caché y accesos nativos ajenos a C. El control existente
`diagnosticar-var-volatile.cmd` reproduce el fallo, pero es diagnóstico, no un
gate completo. No se modifica NSLink ni se afirma resuelto el pendiente.
