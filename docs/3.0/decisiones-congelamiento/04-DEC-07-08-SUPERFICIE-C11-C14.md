# Congelamiento 3.0 DEC-07/DEC-08 — `NSLANGSURFACE 3.0` publica la regla move-only de C-11 y C-14

Fecha: 2026-10-26. Rama: `development-release-candidate`. Revisión de partida: `aa80795b4`.

## Propósito y problema

C-06 (`73150c308`) publicó en `move-only-defaults` la regla implementada en ese momento: «structs whose fields
are all copyable copy implicitly; structs with owned fields, variants and opaque types are move-only and named
consuming reads require move(value)». Después:

- C-11 (`4d6237d84`) hizo move-only a un struct con destructor (`return move(g)`, `Guard y = move(x)`, una copia
  simple es NS3020);
- C-14 (`717c9e5f9`) hizo move-only a los futures, con `await f` consumiendo el local (un segundo `await` es uso de
  un valor movido).

El manifiesto no cambió, así que volvió a ser falso (hallazgo D-06 de la auditoría del congelamiento).

Decisión de Alberto Huerta Luna (2026-10-26): cerrar DEC-07/DEC-08 y dejar `NSLANGSURFACE 3.0` consistente con
C-11 y C-14, antes de fijar los oráculos de C-06. La instrucción presupone que C-11 y C-14 se conservan; DEC-07 y
DEC-08 quedan cerradas con la opción (a) de la auditoría: se conservan con el cambio de contrato autorizado. La
prueba de que la batería histórica no cambia es la corrida integral final, que todavía no se ordena.

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

## Regla implementada (leída del código)

`src/Frontend/MemberModel/OwnershipQueries.cpp`:

- `ProjectMemberModel::typeIsImplicitlyCopyable`: un struct no es copiable si declara destructor (C-11), si tiene
  un campo `owned`, si alguno de sus campos no es copiable, o si lleva la marca `move-only-value` sin `copy`.
  `variant` y `opaque` llevan `move-only-value` desde el parser (`DeclarationParser.cpp`, `setVariantMetadata` y
  `parseOpaqueTypeDeclaration`); el modificador `copy` (`AttributeParser.cpp`) agrega la marca `copy`.
- `ProjectMemberModel::typeIsMoveOnly`: además, todo tipo `future:` es move-only (C-14); tuplas y arreglos fijos
  heredan la condición de sus elementos.

## Sondas (nsc `ecaa727317b7d423`, proyecto con referencia a NativeSharp.Core)

| Sonda | Resultado |
|---|---|
| struct con destructor, `Guard y = x;` | rechazo NS3020 «cannot be copied into local 'y'; use move(source)» |
| struct con destructor, `return move(g)` y reasignación | compila; exit 1212 en -O0 y -O2 |
| `Future<int> g = f;` | rechazo NS3020 «cannot be copied into local 'g'» |
| `await f` dos veces | rechazo NS3020 «use of moved-from move-only local 'f'» |
| `await f` una vez con `owned Box` | compila; exit 44 en -O0 y -O2 (consume y libera el frame) |
| struct de dos `int`, copia y modificación de la copia | compila; exit 7 en -O0 y -O2 (copia implícita) |

## Cambio

`src/Language/Language3Surface.cpp`, entrada `MoveOnlyDefaults`; rationale nueva:

```text
structs without a destructor whose fields are all copyable copy implicitly; structs with a destructor or owned
fields, variants and opaque types without the copy modifier, and futures are move-only; named consuming reads
require move(value) and await consumes its future
```

La disposición (`implemented`), el nombre y el orden no cambian. Se conserva el comentario de C-06 y se agrega uno
bilingüe con DEC-07/DEC-08. Consumidores del texto: `nsc --print-language-3-surface`, `language3SurfaceDiagnostic`
(parser) y las pruebas TL-LANG-155..161 (oráculo fijado en el commit siguiente) y
`tests/unit/NativeSharp3T49LanguageSurfaceTests.cpp`, que sólo exige el prefijo `move-only-defaults=implemented`.

## Hallazgo no autorizado (no implementado)

```text
HALLAZGO NO AUTORIZADO
Evidencia: `nsc --no-banner --no-error-log A:\temp\nsc-dec\probes\p2_c11_move.ns -O0 -o
  A:\temp\nsc-dec\probes\p2_c11_move.exe` (modo archivo, proyecto implícito de C-05, nsc
  `ecaa727317b7d423`) termina con «executable publication failed: artifact free-space preflight failed for
  'p2_c11_move.exe': io-error (Win32 error 123)», también con la ruta de salida relativa. El mismo programa
  compila y corre como proyecto (`--emit-project-oop-exe ... --no-artifact-cache`). Los programas que el
  compilador rechaza en semántica sí reportan su diagnóstico en modo archivo.
Riesgo: `nsc <archivo.ns> -o <exe>` no publica ejecutables (relacionado con N5/N6 de la auditoría).
Componentes afectados: publicación de artefactos del proyecto implícito (C-05).
Posible solución: no determinada; requiere reproducir la ruta que recibe el preflight de espacio libre.
Estado: NO IMPLEMENTAR
```

## Validación

Compilación sobre el árbol final de los cinco commits de este bloque (los commits intermedios no se compilaron por
separado):

- `cmake --build A:/nsc-devel/build-clang --target nsc --parallel 16 -- -k 0`: exit 0, build hash `a75737f72e706419`;
- `cmake --build A:/nsc-devel/build-compiler-suite-v3/debug --config Debug --target nsc_component_tests`: exit 0;
- `cmake --build A:/nsc-devel/build-compiler-suite-v3/release --config Release --target nsc_component_tests`: exit 0.

Pruebas focales (`nsc_component_tests --case <ID>`, Debug y Release), sin batería completa: 42 de 42 PASS
(TD-DIA-003..006, TD-DIA-047..052, TL-LANG-155..161, TL-LANG-339, TL-LANG-376, TL-LANG-413, TP-PAR-561).

`nsc --print-language-3-surface` del nsc recién construido imprime la línea `move-only-defaults=implemented:` con la
rationale nueva. TL-LANG-155..161 PASS en Debug y Release con los oráculos del commit siguiente; en este commit,
aislado, esos casos seguían fallando por C-06.

## No verificado

- Batería histórica completa (criterio de la opción (a) de DEC-07/DEC-08): pendiente de la corrida integral final.
- `Freeze30ContractTests` (C11*, C14*) no se ejecutó en esta máquina; las sondas de arriba reproducen sus casos.
