# Congelamiento 3.0 C-06 — oráculos de superficie TL-LANG-155..161 y TP-PAR-561 en una sola vez

Fecha: 2026-10-26. Rama: `development-release-candidate`. Revisión de partida: `aa80795b4`.

## Propósito y problema

C-06 (`73150c308`) publicó la superficie real de 3.0: async-await, async-streams, yield-generators,
default-interface-implementations y list-patterns pasaron de `planned-3.0.4` a `implemented`, cada uno con su
rationale comprobada, y `case [ ... ]:` pasó a ser un rechazo deliberado (`UnexpectedToken` con
`kListPatternCaseLabelMessage`) en lugar del diagnóstico `NS3049DeferredLanguageSurface`. C-06 actualizó
`tests/unit/NativeSharp3T49LanguageSurfaceTests.cpp`, pero no las pruebas gemelas del suite. Fallaron (reporte
`20261025-222449-571bf9f4`, Debug y Release):

- TL-LANG-155: 6 entradas distintas (las cinco anteriores y `move-only-defaults`);
- TL-LANG-158: bytes del manifiesto;
- TL-LANG-159: diagnóstico de `move-only-defaults`;
- TL-LANG-160: 5 diagnósticos de estado de entradas que ya no están planeadas;
- TL-LANG-161: 6 comparaciones contra la tabla esperada;
- TP-PAR-561, probe `recovery`: bandera `list=0`.

Decisión de Alberto Huerta Luna (2026-10-26): no fijar estos oráculos hasta cerrar DEC-07/DEC-08 y dejar
`NSLANGSURFACE 3.0` consistente con C-11 y C-14 (commit anterior); después, actualizarlos en una sola vez.

## SCOPE LOCK (vigente para las cinco decisiones del 2026-10-26)

- **Objetivo autorizado.** Aplicar las decisiones de Alberto Huerta Luna del 2026-10-26 sobre los
  pendientes del congelamiento 3.0:
  - conservar C-01, C-02, C-13 y C-17 como contratos definitivos;
  - actualizar TL-LANG-339 a ownership explícito sin dejar de probar `new` con tipo inferido;
  - conservar los valores históricos de `DiagnosticId`, agregar los diagnósticos nuevos al final y hacer
    que TD-DIA-006 use un valor realmente fuera del dominio;
  - cerrar DEC-07/DEC-08 y dejar `NSLANGSURFACE 3.0` consistente con C-11 y C-14;
  - después, fijar en una sola vez los oráculos de TL-LANG-155/158/159/160/161 y TP-PAR-561;
  - hacer que TL-LANG-376 valide C-17 de forma determinista, sin depender de `0xE04E5301`.
- **Autorización de pruebas.** Textual: «Te autorizo explícitamente a modificar únicamente las pruebas
  afectadas para alinearlas con esos contratos definitivos, sin retirar ni debilitar cobertura».
- **Naturaleza.** Correctiva y de contrato: un cambio de valores de enum (restauración), un cambio de texto
  de manifiesto y alineación de oráculos de prueba.
- **Comportamiento que debe permanecer idéntico.** Reglas de C-01, C-02, C-11, C-13, C-14 y C-17; nombres
  de todos los diagnósticos; reglas de copia y move-only; parser; codegen; el resto del manifiesto.
- **Revisión de partida.** `aa80795b4` en `development-release-candidate`, árbol limpio.
- **Evidencia de partida.** Corrida de NSC Builder 3 `20261025-222449-571bf9f4` (FAIL=18, 11 causas
  terminales), reproducida caso por caso con `nsc_component_tests --case` en Release el 2026-10-26.
- **Archivos modificables.** `include/nativesharp/Diagnostics/DiagnosticId.h`,
  `src/Language/Language3Surface.cpp`, `tests/compiler-suite/host/modules/DiagnosticIdentityModule.cpp`,
  `tests/compiler-suite/host/modules/LanguageSurfaceModule.cpp`,
  `tests/compiler-suite/host/modules/ParserSwitchCoordinatorCounterfactualModule.cpp`,
  `tests/compiler-suite/manifest/cases.jsonl`, las filas de esos casos en
  `docs/compiler-testing-reengineering/inventory/*.csv` y `test-plans/HOST-DIAGNOSTICS-TEST-PLAN.md`, y
  los documentos de `docs/3.0/decisiones-congelamiento/`.
- **Protegido.** Todo lo demás: analizadores, ownership, async, parser, biblioteca, fixtures históricos,
  `tests/unit`, configuración, CMake y toolchain.
- **Pruebas que deben pasar sin otra modificación.** TL-LANG-413 (await con error dentro de `try`) y los
  demás casos del suite.
- **Prohibido.** Batería completa, baseline, cambios de reglas del compilador, renumerar diagnósticos
  históricos, debilitar oráculos.
- **Criterio de terminación.** Las pruebas focales afectadas pasan en Debug y Release; la batería completa
  queda para la corrida final que Alberto ordene.

## Cambios

`tests/compiler-suite/host/modules/LanguageSurfaceModule.cpp`:

- `kExpectedSurface`: las cinco entradas pasan a `Implemented` con la rationale exacta de
  `src/Language/Language3Surface.cpp`, y `move-only-defaults` toma la rationale de DEC-07/DEC-08. Comentario
  bilingüe: la tabla es el oráculo independiente del catálogo.
- `expectedManifest()`: las mismas seis líneas en bytes exactos, mismo orden y salto de línea final.
- TL-LANG-160: el conteo pasa a 0 planeadas, 0 diferidas y 1 rechazada (`long-double`). El caso sigue comparando
  el diagnóstico exacto de cada entrada no implementada; con la superficie real sólo queda `long-double`. Las ramas
  `PlannedFor304` y `DeferredTo4` de `language3SurfaceDiagnostic` ya no tienen entrada que las use; no se pueden
  ejercitar por la API pública sin cambiar el catálogo.
- TL-LANG-155, 156, 157, 158, 159 y 161 no cambian de código: comparan contra la tabla y el manifiesto de arriba.

`tests/compiler-suite/host/modules/ParserSwitchCoordinatorCounterfactualModule.cpp`, probe `recovery`: el
diagnóstico de `case [1]:` debe ser `UnexpectedToken` con el texto exacto «list patterns are supported in 'match'
and 'is', not in 'case' labels; use a match expression over the value». El texto se escribe literal en la prueba
(oráculo independiente de `kListPatternCaseLabelMessage`). Las otras cinco banderas no cambian.

## Verificación del oráculo contra el catálogo

Antes de escribir los archivos se compiló, fuera del repositorio, un programa con `src/Language/Language3Surface.cpp`
y las tablas `kExpectedSurface`, `expectedManifest` y `expectedDiagnostic` de la prueba: 16 entradas idénticas,
16 diagnósticos idénticos, manifiesto idéntico (2200 bytes), conteos 0/0/1.

## Validación

Compilación sobre el árbol final de los cinco commits de este bloque (los commits intermedios no se compilaron por
separado):

- `cmake --build A:/nsc-devel/build-clang --target nsc --parallel 16 -- -k 0`: exit 0, build hash `a75737f72e706419`;
- `cmake --build A:/nsc-devel/build-compiler-suite-v3/debug --config Debug --target nsc_component_tests`: exit 0;
- `cmake --build A:/nsc-devel/build-compiler-suite-v3/release --config Release --target nsc_component_tests`: exit 0.

Pruebas focales (`nsc_component_tests --case <ID>`, Debug y Release), sin batería completa: 42 de 42 PASS
(TD-DIA-003..006, TD-DIA-047..052, TL-LANG-155..161, TL-LANG-339, TL-LANG-376, TL-LANG-413, TP-PAR-561).

Casos de este commit: TL-LANG-155, 156, 157, 158, 159, 160 y 161 PASS en Debug y Release; TP-PAR-561 (cuatro probes
aislados) PASS en Debug y Release.

## No verificado

- La batería completa no se ejecutó (prohibido en esta etapa).
