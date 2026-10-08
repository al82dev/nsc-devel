# Tanda 8 — Lexer, source y diagnósticos

## Resultado

La tanda incorpora cuatro unidades internas al único ejecutable `nsc_unit_tests.exe` y siete casos CLI al
catálogo externo. Los ejecutables y CTest anteriores permanecen disponibles durante la fase de paridad.

## Unidades C++ consolidadas

| Identificador conservado | Dueño anterior | Módulo nuevo |
|---|---|---|
| `nsc.lexer` | `tests/unit/LexerTests.cpp` | `tests/unit/modules/LexerUnitTests.cpp` |
| `nsc.source-manager` | `tests/unit/SourceManagerTests.cpp` | `tests/unit/modules/SourceUnitTests.cpp` |
| `nsc.diagnostics` | `tests/unit/DiagnosticTests.cpp` | `tests/unit/modules/DiagnosticsUnitTests.cpp` |
| `nsc.ns3.t14.numeric-literals` | `tests/unit/NativeSharp3T14NumericLiteralTests.cpp` | `tests/unit/modules/NumericUnitTests.cpp` |

Cada módulo conserva las aserciones internas sobre tokens, escapes, posiciones, rangos, orden determinista,
presupuesto de errores y resolución de literales. No se sustituyeron por comprobaciones de salida CLI.

## Datos externalizados

- `tests/cases/unit-data/diagnostics/diag.ns` alimenta el renderer consolidado.
- `tests/cases/unit-data/source-manager/disk-source.ns` reemplaza el archivo temporal creado por la unidad
  consolidada y permite que la prueba sólo lea desde A:.
- Siete fuentes de integración se copiaron sin cambios de texto normalizado a sus directorios de caso.
- La equivalencia textual normalizada de las siete copias es `7/7`.

## Casos CLI migrados

- `nsc.dump-tokens`.
- `nsc.invalid.program-missing-main`.
- `nsc.invalid.unknown`.
- `nsc.invalid.bounds`.
- `nsc.invalid.bool-condition`.
- `nsc.invalid.missing-return`.
- `nsc.invalid.unresolved-link`.

Los diagnósticos negativos verifican código de salida y fragmento causal; no comparan rutas temporales,
hashes, banners ni espaciado incidental. El caso de enlace exige una ruta de NSLink explícita.

## Aislamiento de procesos

`NscCommandBuilder` fija `TEMP` y `TMP` al sandbox del caso y permite destinos
`{workspace}/<artefacto>`. El placeholder rechaza rutas que escapen del sandbox. La ejecución de enlace
confirmó que el objeto temporal quedó bajo `A:\nsc-devel\_test-harness\work` y no bajo el perfil de Windows.

## Reclasificación de la matriz inicial

La matriz heurística produjo 37 entradas bajo lexer, source o diagnostics. Once contratos pertenecen al núcleo
de esta tanda. Los otros 26 conservan su propietario y no se pierden:

- `Cc1CorrectnessTests`, features numéricos, layouts, readonly, arrays y comptime: semántica, tanda 10;
- `Item331VariadicCDeclarationTests` y superficie T49: parser/frontend, tanda 9;
- SDK, namespaces, Stage 35/36, Stage 55 y proyectos de recuperación: metadata/proyectos/driver, tanda 14;
- diagnósticos de calidad que dependen del pipeline completo: semántica y driver, tandas 10 y 14.

La clasificación inicial permanece como evidencia histórica; esta tanda no reescribe silenciosamente su JSON.

## Contradicción registrada

El CTest antiguo `nsc.dump-tokens` busca `KwC`. El contrato interno vigente de `LexerTests.cpp` exige que `C`
sea `Identifier` porque `extern C` se resuelve contextualmente, y el `nsc.exe` construido produce precisamente
`Identifier "C"`. El manifiesto nuevo sigue el contrato interno vigente. No se modificó el CTest antiguo ni el
compilador; el estado queda como `DUAL_PARITY_EXPECTATION_DRIFT_RECORDED` hasta el corte.

## Evidencia dirigida

- Construcción de `nsc_unit_tests.exe`: aprobada.
- Unidades consolidadas: `5/5` aprobadas; cuatro pertenecen a esta tanda y una es la identidad migrada antes.
- Unidades legado equivalentes: `4/4` aprobadas.
- Catálogo externo actual: `8/8` aprobado en O0 con `nsc.exe` y NSLink seleccionados desde A:.
- Pruebas Python del harness: `15/15` aprobadas.
- Fuentes copiadas con equivalencia textual normalizada: `7/7`.
- Sandboxes residuales después de la corrida verde: `0`.
- Baseline: no ejecutado.
- O1/O2/O3: no ejecutados.
