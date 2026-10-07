# Tanda correctiva 18: remapeo único de ordinales locales

## SCOPE LOCK

- Objetivo exacto autorizado: corregir regresiones funcionales introducidas por la reingeniería de
  `method-bodies/construction`, preservando el comportamiento válido previo.
- Naturaleza del cambio: corrección interna no funcional del merge determinista de shards semánticos.
- Comportamiento que debe permanecer idéntico: lenguaje, diagnósticos, orden estable, API, ABI, metadata,
  fixtures, proyectos y expectativas históricas.
- Revisión conocida como correcta: comportamiento anterior al rango de reingeniería que termina en
  `72f7ad7cb63f0f31ad51f32631998bcc0b32318b`.
- Rango evaluado: `45dea40ecaed70ed1718df34173edb979aac6db7..72f7ad7cb63f0f31ad51f32631998bcc0b32318b`.
- Archivos modificables: productor `ProjectCallableResultMerger.cpp`, registro CMake, una prueba unitaria nueva
  y este documento.
- Archivos protegidos: todas las pruebas, fixtures, proyectos y expectativas preexistentes; bibliotecas,
  runtime, NSLink, depurador, otros repositorios y cualquier ruta de `C:`.
- Pruebas inmutables: toda prueba histórica permanece byte por byte sin cambios.
- Acciones prohibidas: baseline, minibaseline, O1/O2/O3, instalación o escritura en `C:`, recursión, adaptación
  de consumidores y modificación de expectativas.
- Terminación de esta corrección: un shard posterior al primero remapea cada ordinal local exactamente una vez;
  la prueba nueva demuestra las relaciones bidireccionales entre scopes y locales; compila NSC y pasan las
  comprobaciones O0 dirigidas aplicables.

## Causa raíz demostrada

`ProjectCallableResultMerger::remapNested` ejecutaba `remapCommon` para todo elemento y, cuando el elemento era
`MethodLocalSymbol`, llamaba después a `remapLocal`, que volvía a ejecutar `remapCommon`. Por ello cada shard con
bases globales no nulas sumaba dos veces `scopeBase`, `localBase`, `lambdaBase` y `localBorrowBase` a los campos
correspondientes del local. Los scopes del mismo shard sólo se remapeaban una vez, rompiendo la relación entre
`MethodScope::localOrdinals` y `MethodLocalSymbol::scopeOrdinal`.

El defecto nació en `4cb6167d0a` junto con el merger de resultados aislados. No pertenece al lenguaje ni a los
fixtures: es una divergencia del productor introducida por el refactor de rendimiento.

## Corrección

`remapNested` selecciona ahora una sola ruta:

- `MethodLocalSymbol` usa `remapLocal`, que aplica una vez los campos comunes y una vez el origen de escape;
- cualquier otro registro usa `remapCommon` una vez;
- los registros anidados conservan sus remapeos especializados posteriores.

Se añadió `ProjectCallableResultMergerOrdinalTests.cpp` sin alterar pruebas históricas. El caso compila tres
callables aislados y valida en ambas direcciones que cada local apunta a su scope global y que cada scope apunta a
locales cuyo `scopeOrdinal` coincide exactamente.

## Estado de verificación

- Análisis estático de causa raíz: completado.
- Commit previo a pruebas: `aee41920b` (`Remap callable shard locals exactly once`).
- Build dirigido: completado para los targets del merger, reducer y modelo de cuerpos.
- Prueba nueva: `PASS: 6 checks`.
- Reducer determinista existente, sin modificaciones: `PASS: 5 checks`.
- Modelo completo de method bodies existente, sin modificaciones: `PASS: 76 checks`.
- Batería O0 externa: pendiente del siguiente corte autorizado.
