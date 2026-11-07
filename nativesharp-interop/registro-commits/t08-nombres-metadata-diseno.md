# T08-020: nombres de tipos C importados sólo por metadata precompilada

## Estado

DISEÑO Y PRUEBAS PREPARADOS, NO IMPLEMENTADO. La corrección está en la etapa de parseo compartida de
`ProjectCompilation.cpp`, que decide la gramática de todas las unidades y la selección de tipos precompilados
(trabajo de identidad y caché de Codex). Es una parte compartida protegida y no se modifica en esta rama.

## Síntoma (HECHO COMPROBADO, log `A:\nsc-claude-t08-layout-build\logs\nombres-2.log`)

`pruebas-tanda-08-nombres-metadata` compila el mismo `Consumer.ns` contra una biblioteca precompilada que
publica `NRecord`, el alias `NAlias` y el enum `NMood`:

| Posición del nombre C | Con encabezado (`Shared`) | Sólo metadata |
|---|---|---|
| `NRecord* record = ...;` (`ProbeDecl`) | acepta | rechaza: `unknown identifier 'NRecord'` |
| `sizeof(NRecord)` (`ProbeSizeof`) | acepta | rechaza: `unknown identifier 'NRecord'` |
| `(NRecord*)raw` (`ProbeCast`) | acepta | rechaza: `expected expression` |
| `NRecord copy = *record;` (`ProbeValue`) | acepta | acepta, ejecuta con código 0 |
| `Consumer.ns` completo, O0 | pasó | rechazado en el cast (primer error de parseo) |

Además, en el registro anterior: `offsetof(LNatural, d)`, `LNatural*` en parámetros y tipos de miembros sí
resuelven sin encabezado, porque esas posiciones siempre se parsean como tipo.

## Causa (HECHO COMPROBADO por lectura de código)

1. El parser decide si `T* x`, `sizeof(T)` o `(T*)e` contienen un tipo con `Parser::isKnownTypeName`
   (`src/Parser/TypeParser.cpp`), que consulta `knownTypeNames_`, `sharedKnownTypeNames_` y
   `scopedKnownTypeNames_`.
2. `ProjectCompilation::parseSyntaxUnits` (`src/Frontend/ProjectCompilation.cpp`, cerca de la línea 2627)
   construye esos índices con `parserTypeIndexesByProject(*projectGraph_, units_, {})` y
   `prepareCppScopedParserNames(..., experimentalCppImportedTypes_, ...)`: sólo entran los tipos de las
   sesiones Clang vivas (encabezados).
3. Los tipos C del catálogo precompilado se agregan después del parseo, en la selección
   (`experimentalCppTypeSelector_`, `selectCppTypesWithPrecompiledCatalogs` en `CppSemanticRecord.cpp`), y sólo
   llegan a `ProjectNameLookup`. El parser nunca los ve.

`NRecord copy` funciona porque dos identificadores seguidos ya se leen como declaración sin consultar nombres.

## Dónde deben estar disponibles los nombres

Antes de `parseSyntaxUnits`, en el índice de nombres del parser de cada proyecto que referencia la biblioteca,
con la misma visibilidad que `parserTypeIndexesByProject` aplica a los tipos vivos (cierre de referencias del
proyecto). El conjunto debe salir del mismo catálogo que después usa la selección
(`reconstructPrecompiledCImportedTypes` sobre `precompiledModules()`), incluidos aliases (`NAlias`) y enums, para
que el parser y la búsqueda de nombres no puedan discrepar.

## Diseño propuesto

1. Antes del parseo, reconstruir del catálogo los nombres (no la selección) de records, enums y aliases C de
   cada módulo precompilado y asignarlos al proyecto dueño, como hoy lo hace `selectCppTypesWithPrecompiledCatalogs`.
2. Pasarlos a `prepareCppScopedParserNames` junto con `experimentalCppImportedTypes_`, con las mismas reglas de
   alcance y de conflicto con nombres vivos (misma identidad conciliada = un nombre; identidades distintas =
   el conflicto que ya produce la selección).
3. No adelantar la selección completa ni cambiar `ProjectNameLookup`: sólo el índice del parser.
4. Riesgo e invariante de ausencia: un consumidor sin encabezado que use un identificador igual a un tipo C
   publicado como valor (`NRecord * x`) cambiaría de gramática. Es el mismo comportamiento que hoy tiene el
   consumidor con encabezado; la prueba debe incluir ese caso y su diagnóstico.

## Pruebas preparadas

`nativesharp-interop/pruebas-tanda-08-nombres-metadata/validar.cmd`:

- sondas por posición `ProbeDecl`, `ProbeSizeof`, `ProbeCast` y `ProbeValue`, sólo-metadata, O0; sólo
  imprimen resultados;
- prueba de aceptación: el mismo `Consumer.ns` (declaración, `sizeof`, `alignof`, alias, enum, cast y copia por
  valor, con verificación de valores en C) debe pasar O0–O3 con encabezado y sin él.

Estado actual: `Shared` pasa O0; `Metadata` falla y el script termina con 1. Es el criterio de aceptación de la
corrección pendiente, no una prueba que deba pasar hoy.

## Autorización necesaria

Modificar la preparación de nombres del parser en `ProjectCompilation.cpp` (y, si se elige reutilizar la
reconstrucción, una función nueva de `CppSemanticRecord.cpp`), coordinado con el trabajo de Codex sobre
identidad y selección precompilada.
