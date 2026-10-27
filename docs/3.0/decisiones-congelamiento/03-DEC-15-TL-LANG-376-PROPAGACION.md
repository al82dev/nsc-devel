# Congelamiento 3.0 DEC-15 — TL-LANG-376 prueba la propagación de C-17 con un resultado estable

Fecha: 2026-10-26. Rama: `development-release-candidate`. Revisión de partida: `aa80795b4`.

## Propósito y problema

TL-LANG-376 nació (`484fc459d`, 2026-10-13) con el contrato anterior: un `throw` en el cuerpo async deja el
future en error (`state=-2`) y el `await` bloqueante devolvía `-1`. C-17 (`bc8fca456`) cambió ese contrato: un
`await` síncrono fuera de `try` **propaga** el error del future como excepción. Como el `Main` del caso declaraba
`throws Exception`, el error salía de `Main` y el proceso terminaba con 3763229441 = `0xE04E5301`, el código
nativo de una excepción no atrapada (causa 9 del reporte `20261025-222449-571bf9f4`, Debug y Release).

Decisión de Alberto Huerta Luna (DEC-15, 2026-10-26): se conserva la propagación de C-17. TL-LANG-376 debe
validarla de forma determinista y no congelarse dependiendo de `0xE04E5301`, porque el reporte de errores de
Windows demoró esa ruta de forma intermitente en `-O2` (`NSC-ESTADO-CONGELAMIENTO-3.0.md`, limitación 10).

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

## Cambio

`tests/compiler-suite/host/modules/LanguageSurfaceModule.cpp`, `registerAsyncThrowErrorCase`:

```text
public static async int Read() throws Exception { throw new Exception(); return 42; }
public static int Forward() throws Exception { int result = await Read(); return result + 100; }
public static int Main() { try { return Forward(); } catch (Exception e) { return 9; } }
```

- `Read` sigue siendo el mismo cuerpo async que lanza (el error queda en el future).
- `Forward` hace el `await` síncrono **fuera de todo `try`**: es exactamente la conducta de C-17.
- `Main` atrapa y devuelve 9. El 9 sólo se alcanza por el `catch`: si el `await` devolviera el valor (142) o
  un valor por defecto (100) el caso falla, y ninguna ruta depende de una excepción no atrapada.
- TL-LANG-413 (await dentro de `try` en el mismo callable) queda igual; los dos casos cubren las dos posiciones
  del `await` respecto del `try`.
- Oráculo: compila y termina 9. Comentario bilingüe con la razón.
- `cases.jsonl` (entrada y oráculo `exit: 9`) y las filas del caso en `test-case-design.csv` y
  `suite-case-allocation.csv`.

## Sondas previas al cambio (nsc `ecaa727317b7d423`, proyecto con referencia a NativeSharp.Core)

| Programa | -O0 | -O2 |
|---|---|---|
| Programa nuevo del caso | compila, exit 9 | compila, exit 9 |

## Validación

Compilación sobre el árbol final de los cinco commits de este bloque (los commits intermedios no se compilaron por
separado):

- `cmake --build A:/nsc-devel/build-clang --target nsc --parallel 16 -- -k 0`: exit 0, build hash `a75737f72e706419`;
- `cmake --build A:/nsc-devel/build-compiler-suite-v3/debug --config Debug --target nsc_component_tests`: exit 0;
- `cmake --build A:/nsc-devel/build-compiler-suite-v3/release --config Release --target nsc_component_tests`: exit 0.

Pruebas focales (`nsc_component_tests --case <ID>`, Debug y Release), sin batería completa: 42 de 42 PASS
(TD-DIA-003..006, TD-DIA-047..052, TL-LANG-155..161, TL-LANG-339, TL-LANG-376, TL-LANG-413, TP-PAR-561).

Casos de este commit: TL-LANG-376 PASS en Debug y Release; TL-LANG-413 (sin cambios) PASS en Debug y Release.

## No verificado

- La batería completa no se ejecutó (prohibido en esta etapa).
