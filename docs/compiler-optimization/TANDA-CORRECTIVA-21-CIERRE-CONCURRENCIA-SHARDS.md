# Tanda correctiva 21: cierre de concurrencia de shards

## SCOPE LOCK

### Objetivo

Cerrar por analisis estatico el inventario de estado compartido por los shards de method bodies,
despues de corregir el doble remapeo de ordinales y la cache mutable de ownership.

### Alcance

- Solo auditoria y documentacion dentro de `A:\nsc-devel`.
- Sin cambios de codigo, pruebas historicas, fixtures, proyectos ni expectativas.
- Sin baseline, minibaseline, CTest, instalacion o acceso a `C:\native-sharp`.

## Inventario verificado

`ProjectCallableAnalysisContext` entrega a cada worker referencias `const` a:

- `ProjectNameLookup`;
- `ProjectMemberModel`;
- `ProjectNamespaceFunctionModel`;
- `ProjectInheritanceModel`;
- `ProjectInterfaceModel`;
- `ProjectGenericModel`;
- `ProjectGenericConstraintModel`.

Ninguno de esos siete modelos conserva ya un campo `mutable`. La cache de ownership que antes se
llenaba desde una consulta `const` se completa ahora, de forma iterativa, durante
`ProjectMemberModel::build()` y es de solo lectura cuando comienzan las waves.

Cada invocacion de `analyzeCallableIsolated()` crea un `ProjectMethodBodyModel` independiente y un
`DiagnosticEngine` local. Por tanto, los contenedores de scopes, locals, bindings, ownership,
escape, estadisticas y diagnostics no se comparten entre workers.

`ProjectCallableWaveScheduler` asigna un slot exclusivo por callable. El coordinador espera la
terminacion completa del pool, publica los slots en orden estable y solo entonces entrega los
resultados a `ProjectCallableWaveReducer` y `ProjectCallableResultMerger`.

## Candidatos descartados

- `PrecompiledMemberIdentityIndex::lookupCount_`: se usa durante member build, antes de crear
  shards, y el indice es local a esa operacion.
- `ProjectMetaprogrammingModel::visibleScratch_`: el modelo se construye despues de method bodies y
  no forma parte de `ProjectCallableAnalysisContext`.
- Estadisticas `mutable` de `ProjectMethodBodyModel`: cada instancia pertenece a un unico shard.
- `ComptimeEngine`: su cache compartida esta protegida por mutex y pertenece a la fase de syntax,
  no a las waves de callables.

## Pruebas historicas

La comparacion `origin/development...HEAD` confirma que esta secuencia no modifico ningun archivo
de prueba existente. Solo agrego:

- `tests/unit/ProjectCallableResultMergerOrdinalTests.cpp`;
- `tests/unit/ProjectMemberOwnershipCacheTests.cpp`;
- `tests/unit/ProjectMemberTypeWorklistTests.cpp`.

## Estado

AUDITORIA ESTATICA CERRADA.

La bateria O0 completa posterior a estos commits corresponde a la validacion externa del usuario;
no se ejecuta desde esta tanda.
