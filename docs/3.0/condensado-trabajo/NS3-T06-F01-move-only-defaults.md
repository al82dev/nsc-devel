# NS3-T06-F01 — Move-only defaults

## Resultado

NativeSharp 3.0 ya no considera copiables todos los structs por accidente. Los tipos valor de
edición 3 son move-only por defecto y optan a copia con `copy`; el estado moved/maybe-moved es
path-sensitive y una reasignación completa vuelve a habilitar el nombre.

## Cambios

- Parser: acepta `copy struct`, `copy record struct`, `copy union`, `copy variant` y
  `copy opaque type`, incluidos modificadores combinados y tipos anidados; edición 2 no adquiere
  esta semántica.
- Member model: una sola consulta transitiva decide Copy/move-only para fields, fixed arrays y
  constraints; rechaza promesas Copy falsas.
- Method bodies: inicialización, parámetros, funciones libres, delegates/eventos, constructores,
  returns y asignación exigen `move(...)` al consumir almacenamiento move-only. Branches y loops
  reutilizan los joins moved/maybe-moved existentes; la reasignación rehabilita el valor.
- Genéricos: `where T : copy` usa la capacidad declarada en edición 3 y conserva la regla
  estructural histórica para edición 2.
- Bibliotecas: NSMETA sube a 1.33, publica `kMetadataTypeCopy`, rechaza downlevel con pérdida y
  reconstruye `copy` al consumir una `LibraryReference`.
- Superficie: `--print-language-3-surface` publica `move-only-defaults=implemented`.

## Archivos nuevos

- `tests/unit/NativeSharp3T06MoveOnlyTests.cpp`: prueba dirigida autocontenida; existe para cubrir
  semántica, flujo, diagnósticos, O0/NSLink y frontera de biblioteca sin ejecutar el baseline.
- `tests/fixtures/nsmeta/historical-1.32.nsmeta`: golden inmutable del formato inmediatamente
  anterior; prueba que el lector 1.33 conserva la compatibilidad binaria hacia atrás.
- `docs/3.0/language/move-only-copy.md`: contrato ES/EN y guía de migración 2.x → 3.0.
- Este archivo: evidencia y mapa del commit exigido por la metodología del proyecto.

## Archivos modificados

- `include/nativesharp/{Parser/Parser.h,Frontend/ProjectMemberModel.h,Frontend/ProjectMethodBodyModel.h,Frontend/ProjectConstructionModel.h,Interop/LibraryMetadata.h,Language/Language3Surface.h}`.
- `src/{Parser/Parser.cpp,Frontend/ProjectMemberModel.cpp,Frontend/ProjectGenericConstraintModel.cpp,Frontend/ProjectMethodBodyModel.cpp,Frontend/ProjectConstructionModel.cpp,Interop/LibraryMetadata.cpp,Driver/Driver.cpp,Driver/BreakingChangeLedger.cpp,Driver/ReleaseCandidateFreeze.cpp,Language/Language3Surface.cpp}`.
- `CMakeLists.txt` y pruebas/roadmaps directamente afectados por el nuevo contrato y NSMETA 1.33.

## Mejora observable

El compilador impide copias silenciosas que duplicarían identidad o recursos, conserva un camino
explícito y auditable para transferir valores, y mantiene la misma decisión dentro del proyecto,
en genéricos y al cruzar `.lib/.nsmeta`. El layout y ABI Win64 no cambian.

## Validación dirigida

- Compilación incremental del target `nsc`: correcta con warnings-as-errors.
- Matriz dirigida acumulada: 23/23 verde en 20.83 s, incluida ejecución O0/NSLink,
  `nsc.ns3.t06.move-only-defaults`, T01/T03/T04/T05, superficie 3.0, NSMETA 1.33 y contratos
  versionados de compatibilidad/promoción.
- `nsc.stage48_2.library-metadata`: verde nuevamente sin modo de actualización después de archivar
  el golden 1.32; ninguna evidencia histórica anterior fue reescrita.
- Baseline: no ejecutado por política explícita del proyecto.
