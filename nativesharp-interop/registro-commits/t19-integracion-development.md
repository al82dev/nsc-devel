# Tanda 19: integración de `development` en la rama de interoperabilidad (2026-11-18)

## Qué se integró

- Commit de integración: `61eb49519` («Merge development into development_cpp__clang_integracion»), sobre la
  rama `development_cpp__clang_integracion` (base `000ccc423`, la punta de `development-cpp-clang`), con
  `development` en `2653041`. Auditoría previa: [`migración C language.md`](../../migración%20C%20language.md).
- Después del merge, `development` en `2653041` es ancestro de la rama y no quedan marcadores de conflicto
  en `src`, `include`, `lib`, `selfhost` ni `cmake`.

## Cómo quedó lo que afecta a la interoperabilidad

| Tema | Resultado |
|---|---|
| NSMeta | Sin versión de formato: se valida por estructura y contenido. Los datos C del destino (`cppBinaryDependencies`, `cppCRecordCatalog`) se conservan. No se reintrodujeron contadores ni diagnósticos de versión. |
| Versión del lenguaje y del producto | Sin `kLanguageVersion`; producto 1.0. |
| `Float64x2` de NativeSharp.Core.Numerics (portadores de vectores C, T19-036) | Movido a NativeSharp Libraries (`d8de3ca` en `A:\nativesharp-libraries`). |
| Bibliotecas estándar | Trasladadas a NativeSharp Libraries; `lib/` conserva proyectos que redirigen. Auditoría: [`AUDITORIA-TRASLADO-BIBLIOTECAS.md`](../../AUDITORIA-TRASLADO-BIBLIOTECAS.md). |
| Prueba T09-003 `nsc.cpp-interop.clang-abi-probe` | Fallaba con salida 209, también en `000ccc423`: seguía esperando el rechazo de cuatro records que T19-030 empezó a admitir a propósito. Actualizada en `8b3325296`; las exclusiones restantes se mantienen. |
| Carpetas de prueba con sello de tiempo | Las pruebas `clang-abi-probe`, `clang-variadic-pointer-graph` y `record-value-observation` dejaban una carpeta en el checkout; desde `8b3325296` la borran al terminar. |

## Validación ejecutada (compilación incremental y pruebas dirigidas, CMD)

Build `A:\nsc-devel-fixes\build-integracion` (interop ON, pruebas ON).

| Ejecución | Pruebas `cpp-interop` | Resultado |
|---|---|---|
| Tanda dirigida, 95 pruebas (09:31) | 10 | 8 pasan; fallan `clang-abi-probe` y `session`. |
| Después de `8b3325296` | `clang-abi-probe`, `clang-variadic-pointer-graph`, `record-value-observation` | Pasan y no dejan carpetas. |
| Tanda dirigida, 65 pruebas (10:29) | — | `nsc.stage46-1.dll-cross-linker-matrix` pasa. En la tanda de las 09:31 había fallado; no se aisló la causa. |

`nsc.cpp-interop.session` sigue fallando por la misma causa ya registrada en T19-037: su árbol de fixtures
(`cpp-fixtures`) no existe en la raíz de prueba. No es una regresión de la integración.

Registros: `A:\temp\merge-audit\ctest-targeted.log` y `A:\temp\merge-audit\ctest-set2.log`.

## Pendiente (sin evidencia todavía)

- **Batería completa**: no se ha ejecutado tras la integración. Se correrá cuando el usuario lo indique.
- **Plan de validación** de la sección «Validación que debe planearse» de la auditoría. Con las pruebas
  dirigidas no queda cubierto como matriz propia:
  - lectura y escritura de NSMeta sin versión, con y sin datos C;
  - negativos de truncamiento y de catálogos inconsistentes;
  - tipos y funciones C afectados en O0–O3;
  - regresiones de ownership y vistas al invocar funciones importadas.
- Ningún ítem de la tanda 19 cambia de estado por esta integración.
