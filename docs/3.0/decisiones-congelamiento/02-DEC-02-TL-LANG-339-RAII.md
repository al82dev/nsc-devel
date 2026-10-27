# Congelamiento 3.0 DEC-02 — TL-LANG-339 recibe el `new(...)` con un dueño `raii` explícito

Fecha: 2026-10-26. Rama: `development-release-candidate`. Revisión de partida: `aa80795b4`.

## Propósito y problema

C-01 (`ea0da601c`) convirtió en error de compilación (`LostOwnership`) un `new` cuyo resultado llega a un local
sin `owned` ni `raii`. TL-LANG-339 (`T09TargetTypedNew`) declaraba `Point p = new(3, 4);` y falló en Debug y
Release (causas 7 y 8 del reporte `20261025-222449-571bf9f4`):

```text
T09TargetTypedNew.ns:18:19: error: the object created by 'new' reaches no owner through the non-owning
local 'p' and would leak; store it in a raii or owned local, pass it to an owned parameter, or return it
through an owned return type
```

No es un defecto del `new` con tipo inferido: el tipo `Point` se infiere del destino y la regla es la misma que
para `Point p = new Point(3, 4);`.

Decisión de Alberto Huerta Luna (DEC-02, 2026-10-26): un `new` asignado a un local sin `owned`/`raii` es error
de compilación; TL-LANG-339 se actualiza para usar ownership explícito sin dejar de probar target-typed new.

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

- `tests/compiler-suite/host/modules/LanguageSurfaceModule.cpp`, `registerTargetTypedNewCase`: el programa usa
  `raii Point p = new(3, 4);`. El inicializador sigue siendo `new(3, 4)` sin nombre de tipo, así que el caso
  sigue probando que el tipo sale sólo del destino, y el oráculo (compila; el programa termina 7) no cambia.
  Comentario bilingüe con la razón.
- `tests/compiler-suite/manifest/cases.jsonl` y la fila del caso en `test-case-design.csv`: la entrada registrada
  pasa a `raii Point p = new(3, 4);`.

Se eligió `raii` porque es el dueño local que el suite ya usa para objetos (`raii Item item = new Item();` y
otros); `owned` también es válido para la regla, pero no agrega nada al caso.

## Validación

Compilación sobre el árbol final de los cinco commits de este bloque (los commits intermedios no se compilaron por
separado):

- `cmake --build A:/nsc-devel/build-clang --target nsc --parallel 16 -- -k 0`: exit 0, build hash `a75737f72e706419`;
- `cmake --build A:/nsc-devel/build-compiler-suite-v3/debug --config Debug --target nsc_component_tests`: exit 0;
- `cmake --build A:/nsc-devel/build-compiler-suite-v3/release --config Release --target nsc_component_tests`: exit 0.

Pruebas focales (`nsc_component_tests --case <ID>`, Debug y Release), sin batería completa: 42 de 42 PASS
(TD-DIA-003..006, TD-DIA-047..052, TL-LANG-155..161, TL-LANG-339, TL-LANG-376, TL-LANG-413, TP-PAR-561).

Caso de este commit: TL-LANG-339 PASS en Debug y Release. Sonda previa con el mismo programa en un proyecto con
referencia a NativeSharp.Core (nsc `ecaa727317b7d423`): compila y termina 7 en -O0 y -O2.

## No verificado

- La batería completa no se ejecutó (prohibido en esta etapa).
