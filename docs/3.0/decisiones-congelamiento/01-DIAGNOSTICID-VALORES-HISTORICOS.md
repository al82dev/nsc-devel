# Congelamiento 3.0 — `DiagnosticId` conserva sus valores históricos (TD-DIA-003..006)

Fecha: 2026-10-26. Rama: `development-release-candidate`. Revisión de partida: `aa80795b4`.

## Propósito y problema

C-01, C-02 y C-13 agregaron tres diagnósticos (`LostOwnership`, `SynchronousAwaitCanSuspend`,
`UnhandledDeclaredError`) **entre** `InvalidExternCSignature` y `NS3010InvalidCompilationStage`. Con eso
los cinco códigos cortos pasaron de los valores 69..73 a 72..76 y el valor 74, que TD-DIA-006 usaba como
«desconocido», quedó ocupado por `NS3030MissingEffectContext`: el nombre devuelto fue `NS3030` en vez de
`UnknownDiagnostic` y el valor quedó clasificado como código corto. Esa fue la causa 1 del reporte de
NSC Builder 3 `20261025-222449-571bf9f4` (TD-DIA-006 en Debug y Release).

Decisión de Alberto Huerta Luna (2026-10-26): conservar los valores históricos, agregar los diagnósticos
nuevos al final, no renumerar los anteriores y hacer que TD-DIA-006 use un valor realmente fuera del
dominio. Las mismas instrucciones conservan C-01 (DEC-02), C-02 y C-13 como contratos definitivos, así que
los tres diagnósticos se quedan.

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

## Cadena causal

| Paso | Evidencia |
|---|---|
| Entrada | `static_cast<DiagnosticId>(74)` en TD-DIA-006 |
| Operación | `diagnosticIdName`, `isNativeSharp3Diagnostic` |
| Valor antes del congelamiento | 74 fuera del dominio (69 ordinarios + 5 cortos = 0..73) |
| Valor en `aa80795b4` | 74 = `NS3030MissingEffectContext` (comprobado compilando el encabezado) |
| Símbolo responsable | orden de los enumeradores en `include/nativesharp/Diagnostics/DiagnosticId.h` |
| Corrección | mover los tres enumeradores después de `NS3099InvalidConformanceManifest` |

## Cambios

- `include/nativesharp/Diagnostics/DiagnosticId.h`: `LostOwnership` = 74, `SynchronousAwaitCanSuspend` = 75,
  `UnhandledDeclaredError` = 76; `NS3010..NS3099` vuelven a 69..73 y `CannotOpenSourceFile..InvalidExternCSignature`
  siguen en 0..68. Los nombres y `isNativeSharp3Diagnostic` no cambian. Comentario bilingüe que fija la regla:
  los IDs nuevos se agregan al final.
- `tests/compiler-suite/host/modules/DiagnosticIdentityModule.cpp`:
  - el catálogo independiente incorpora los tres IDs del congelamiento (TD-DIA-003: 77 nombres exactos y
    únicos; TD-DIA-004: 72 ordinarios);
  - TD-DIA-004 comprueba además el valor numérico de cada ID ordinario (0..68 y 74..76) y TD-DIA-005 el de
    cada código corto (69..73), para que una inserción futura en medio del enum falle en estas pruebas;
  - TD-DIA-006 usa `std::numeric_limits<int>::max()`, que queda fuera del dominio aunque se agreguen más IDs
    al final. El `NOLINTNEXTLINE` del caso se conserva.
- `tests/compiler-suite/manifest/cases.jsonl`: entradas de TD-DIA-003 (77), TD-DIA-004 (72) y TD-DIA-006
  (2147483647).
- Filas de TD-DIA-003/004/005 en `docs/compiler-testing-reengineering/inventory/test-case-design.csv` y en
  `docs/compiler-testing-reengineering/test-plans/HOST-DIAGNOSTICS-TEST-PLAN.md`.

## Consumidores del valor numérico

Búsqueda de `static_cast<...>(...id())` y `DiagnosticId>(` en `src`, `include`, `tools` y `tests`:

- `src/Diagnostics/DiagnosticEngine.cpp:199/207` usa el valor sólo como desempate al ordenar diagnósticos con la
  misma ubicación, extremo y severidad. Con este cambio el orden relativo de los diagnósticos históricos vuelve
  a ser el anterior al congelamiento; los tres nuevos quedan después de los códigos cortos.
- `DiagnosticRendererHeaderModule.cpp` usa 999 como desconocido; sigue fuera del dominio.
- Ningún archivo serializa el valor numérico; los integradores usan el nombre (comentario del propio encabezado).

## Compatibilidad, riesgos y austeridad

- Un desempate entre un diagnóstico nuevo y un `NS30xx` en el mismo punto cambia de orden respecto de
  `aa80795b4`; respecto del estado previo al congelamiento no hay cambio. No se encontró una prueba que dependa
  de ese orden.
- Sin recursión, sin lógica nueva en el compilador. `ordinaryValue` es O(1).

## Validación

Compilación sobre el árbol final de los cinco commits de este bloque (los commits intermedios no se compilaron por
separado):

- `cmake --build A:/nsc-devel/build-clang --target nsc --parallel 16 -- -k 0`: exit 0, build hash `a75737f72e706419`;
- `cmake --build A:/nsc-devel/build-compiler-suite-v3/debug --config Debug --target nsc_component_tests`: exit 0;
- `cmake --build A:/nsc-devel/build-compiler-suite-v3/release --config Release --target nsc_component_tests`: exit 0.

Pruebas focales (`nsc_component_tests --case <ID>`, Debug y Release), sin batería completa: 42 de 42 PASS
(TD-DIA-003..006, TD-DIA-047..052, TL-LANG-155..161, TL-LANG-339, TL-LANG-376, TL-LANG-413, TP-PAR-561).

Casos de este commit: TD-DIA-003, 004, 005 y 006 PASS en Debug y Release; TD-DIA-047..052 (renderer de encabezados,
consumidor de los nombres) PASS en Debug y Release.

Comprobación del valor de cada enumerador compilando el encabezado fuera del repositorio: antes `NS3010`=72,
`NS3099`=76, 74=`NS3030`; después `InvalidExternCSignature`=68, `NS3010`=69, `NS3099`=73, `LostOwnership`=74,
`SynchronousAwaitCanSuspend`=75, `UnhandledDeclaredError`=76; ningún valor de 0..76 sin nombre y 77 devuelve
`UnknownDiagnostic`.

## No verificado

- La batería completa no se ejecutó (prohibido en esta etapa).
- `tests/unit` no se ejecutó en esta máquina.
