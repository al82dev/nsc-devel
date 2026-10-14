# NativeSharp 3.0.4 — Handover de Parte B

## 1. Propósito y autoridad

Este documento conserva el punto de reanudación autosuficiente de la Parte B,
NativeSharp 3.0.4, después de cerrar `NS304-T03-F03`. No certifica la Parte B,
no sustituye el tracking maestro y no autoriza adelantar trabajo posterior.

Fuentes canónicas, en orden de autoridad:

1. `A:\LINEAMIENTOS-DE-TRABAJO-Y-PORTEO.md`.
2. `A:\NATIVESHARP-V3.0.4-PLAN-DE-DESARROLLO.md`.
3. `A:\nsc-devel\NATIVESHARP-3.0.3-A-3.0.6-TRACKING-MAESTRO.md`.
4. `A:\nsc-devel\docs\3.0.4\items\`.
5. `A:\nsc-devel\docs\3.0.4\contracts\`.
6. Código, manifiestos, pruebas y evidencia versionados por cada commit.

Los lineamientos aplicables a este corte tienen 5,109 líneas lógicas y SHA-256
`DE896530ED5D0F4DDB1D4DAA42635F921563C7C764060CEB2D781629C77C3D4F`.

## 2. SCOPE LOCK del handover

Objetivo: preservar el estado verificable de Parte B en 25/147, reconciliar el
handover original con el cierre posterior de F03 y dejar una reanudación
inequívoca desde F04.

Naturaleza: documentación del último estado comprometido y de su evidencia.

Invariantes:

- `T00`, `T01`, `T02` y `T03-F01..F03` permanecen cerrados.
- `T03-F04` es el siguiente ítem secuencial; ningún resultado posterior se usa
  para aumentar el conteo oficial antes de cerrar su orden de campaña.
- No se cambia compilador, CMake, toolchain, Builder ni configuración.
- No se ejecuta build, prueba ni batería como parte del handover.
- No se modifica, incluye ni revierte trabajo ajeno.
- La rama activa de trabajo continúa siendo `development`.
- `development-ChatGPT` es una frontera de handover, no una rama de versión.

Criterio de consistencia de este handover: conteo 25/147, T03 3/8, evidencia de
F03 trazada y siguiente acción T03-F04 sin afirmar certificación global.

## 3. Estado ejecutivo

> **Reconciliación (2026-10-14):** este handover es un corte histórico en 25/147. El estado vigente
> es **60/147 (40.82 %)** según el tracking maestro (`NATIVESHARP-3.0.3-A-3.0.6-TRACKING-MAESTRO.md`),
> con T00-T04 cerrados y T05-T10 con ítems cerrados (T07-F01/F02, T08-F01..F04, T09-F05, T10-F02/
> F04/F07). La tabla siguiente conserva el corte original.

| Dimensión | Estado al corte |
|---|---|
| Parte B | `PAUSADA POR INSTRUCCIÓN DEL USUARIO` |
| Plan completo | 147 ítems, T00–T17 |
| Cerrados (al corte) | 25/147 |
| Cerrados (vigente) | 60/147 (ver tracking maestro) |
| Pendientes (vigente) | 87/147 |
| T00 | 8/8, cerrado |
| T01 | 8/8, cerrado |
| T02 | 6/6, cerrado |
| T03 | 8/8, cerrado (F01–F08) |
| Siguiente ítem (al corte) | `NS304-T03-F04` |
| Batería completa de Parte B | No ejecutada; reservada para después de T17 |
| Rama de trabajo | `development` |
| Último cierre secuencial | `691661767ce15a158492ff36eb65335654c8f0ab` (`NS304-T03-F03`) |

El conteo se obtiene exclusivamente de los estados cerrados del tracking:
T00 8 + T01 8 + T02 6 + T03 3 = 25. No se atribuye avance a documentación,
auditorías o propuestas pendientes.

## 4. Metodología persistente de reanudación

Para cada ítem restante se conserva el flujo vinculante:

1. Releer los lineamientos completos y el SCOPE LOCK vigente.
2. Confirmar `development`, HEAD, worktree y cambios ajenos.
3. Auditar contrato, productor, consumidores y dependencias antes de editar.
4. Implementar una sola causa cohesionada mediante la corrección mínima.
5. Construir únicamente productores e inputs afectados, sin regenerar ni
   cambiar configuración salvo autorización explícita posterior.
6. Ejecutar sólo la prueba puntual del ítem y sus guards causales.
7. Actualizar expediente, tracking, manifiesto e inventarios relacionados.
8. Crear un commit detallado, aislado y con evidencia reproducible.
9. Continuar al siguiente ítem en orden sólo después del PASS requerido.
10. Al cerrar la tanda, ejecutar su consolidado exacto; no ejecutar batería
    completa entre ítems ni entre tandas.
11. Después del último ítem de T17, ejecutar una sola batería completa de
    certificación. Si aparecen errores atribuibles al alcance, reabrir desde
    T00, auditar todas las tandas, corregir dirigidamente y repetir la batería
    completa sólo al final del nuevo ciclo.

Una prueba puntual valida su ítem; no certifica la Parte B. Un build focal no
equivale a batería completa. Ningún estado pasa a cerrado sin evidencia.

## 5. T00 — autoridad, inventarios y harness, 8/8

| Ítem | Commit | Evidencia de cierre |
|---|---|---|
| T00-F01 | `69f2d2b3e` | 55/55 expedientes y commits trazados; omitidos=0 |
| T00-F02 | `5398f0944` | 55/55 filas por capas; inferencias históricas=0 |
| T00-F03 | `816ce1a88` | 88/88 capacidades C# 8–14 clasificadas |
| T00-F04 | `67ee19257` | 69/69 capacidades Beef clasificadas |
| T00-F05 | `73019e590` | `TL-LANG-155..161`, 7/7 PASS |
| T00-F06 | `f26e51ff4` | seis contratos de versión, 6/6 consistentes |
| T00-F07 | `f1d1a69db` | 18/18 familias con owners y dependencias |
| T00-F08 | `dbc5e4289` | 4/4 unitarias y gate real 147/147 PASS |

El commit `08c19e4d5` reconcilió previamente el universo canónico de 147 ítems.
El consolidado T00 cerró F01=55, F02=55, F03=88, F04=69, F05=7/7,
F06=6, F07=18 y F08=4/4 más el gate real, sin batería completa.

## 6. T01 — inicialización estática, 8/8

| Ítem | Commit | Prueba principal y evidencia |
|---|---|---|
| T01-F01 | `b2a933a20` | `TL-LANG-294` PASS; estados cerrados 0/1/2/3 |
| T01-F02 | `bd8f28eaf` | `TL-LANG-295` PASS; cinco negativos exactos |
| T01-F03 | `2c1a3333e` | `TL-LANG-296` PASS; orden Producer/Consumer |
| T01-F04 | `e5cf52caf` | `TL-LANG-297` PASS; guard LLVM 61 checks |
| T01-F05 | `00e50faa9` | `TL-LANG-298` PASS; fallo terminal sin retry |
| T01-F06 | `b42f55670` | `TL-LANG-299` PASS; guard LLVM 72 checks |
| T01-F07 | `2bcaa47aa` | `TL-LANG-300` PASS; guards Parser 6/6 |
| T01-F08 | `036d08814` | `TL-LANG-301` PASS; consolidado 294–301, 8/8 |

El cierre integró metadata NSMETA, debug NSDEBUG/PDB, ABI y coordinación de
constructores estáticos. Los guards `TL-LANG-155..161` fueron 7/7 PASS y
`TF-INTEROP-273` PASS. Los builds fueron focales con LLVM/Clang 23 y no se
regeneró CMake.

## 7. T02 — implementaciones default, 6/6

| Ítem | Commit | Prueba principal y evidencia |
|---|---|---|
| T02-F01 | `1d619ef44` | `TL-LANG-302` PASS; Parser 14/14 y 271–278 8/8 |
| T02-F02 | `14a9cf6d9` | `TL-LANG-303` PASS; precedencia coherente |
| T02-F03 | `a8889fb75` | `TL-LANG-304` PASS; diamantes y coherence |
| T02-F04 | `0ebe34f85` | `TL-LANG-305` PASS; generic/associated/some/dyn |
| T02-F05 | `7b160be3b` | `TL-LANG-306` PASS; witnesses NSMETA |
| T02-F06 | `2bb1cb117` | `TL-LANG-307` PASS; ABI, DLL, vtable y backend |

Consolidado T02: `TL-LANG-302..307`, 6/6 PASS; guards
`TL-LANG-271..278`, 8/8 PASS. Los builds fueron focales con LLVM/Clang 23,
sin regenerar CMake y sin batería completa.

## 8. T03 — patterns, 3/8

### T03-F01 — formas base

- Commit: `7c1aa02ee`.
- Expediente: `docs/3.0.4/items/NS304-T03-F01-list-pattern-shapes.md`.
- Contrato: exacto, prefijo, sufijo, rest y descarte preservan longitud y orden.
- Prueba: `TL-LANG-308` PASS en 361243 microsegundos; exit 42.
- Guards: `TL-LANG-261..270`, 10/10 PASS.
- Build focal: compilador 11/11 y runner 70/70 con LLVM/Clang 23.

### T03-F02 — rest/slice prestado

- Commit: `fe6b36edd`.
- Expediente: `docs/3.0.4/items/NS304-T03-F02-rest-slice-capture.md`.
- Contrato: `.. <pattern>` produce `Slice(prefix, Length-fixed)` sin copia y
  conserva lifetime prestado.
- Prueba: `TL-LANG-309` PASS en 388498 microsegundos; positivo exit 52.
- El positivo permanece en `TL-LANG-309`; el negativo de escape se aisló
  posteriormente como `TL-LANG-365` dentro del cierre F03.
- Guards: `TL-LANG-308` y `TL-LANG-261..270`, 11/11 PASS.
- Build focal: `Parser.cpp`, frontend de 142 objetos, enlace nsc de 98 inputs;
  runner 69+2+2 aristas con LLVM/Clang 23, sin regenerar CMake.

### T03-F03 — sustratos y vistas rest

- Estado: cerrado y contabilizado como el ítem 25/147.
- Expediente: `docs/3.0.4/items/NS304-T03-F03-list-pattern-collections.md`.
- Matriz base: `TL-LANG-310` y `TL-LANG-357..362`, 7/7 PASS, para string,
  arrays, inline arrays, Span, ReadOnlySpan, StringView y protocolo usuario.
- Bordes de cierre: `TL-LANG-363` cubre UTF-8 no ASCII; `TL-LANG-364`
  demuestra alias/no-copia observable y readonly; `TL-LANG-365` rechaza el
  escape de una vista rest derivada de `stackalloc` con diagnóstico exacto.
- Infraestructura causal: claves de especialización cualificadas, cola
  determinista de materialización desde cuerpos, accessors `readonly get` y
  lowering de vistas de fixed arrays quedaron cubiertos por pruebas focales.
- Cierre: `691661767ce15a158492ff36eb65335654c8f0ab`; no hubo batería completa.

La siguiente acción exacta es `NS304-T03-F04`. Los resultados adelantados o
auditorías de F04 en adelante no aumentan por sí solos el conteo secuencial.

## 9. Configuración y toolchain congelados

- NativeSharp utiliza exclusivamente LLVM/Clang 23.x desde la raíz oficial
  configurada `C:\clang-llvm`.
- `clang-cl`, `lld-link`, `llvm-rc`, `llvm-mt` y `LLVM_DIR` deben proceder de
  esa misma raíz. No se permiten PATH, registry, MSYS ni fallbacks.
- Los árboles y aristas existentes son evidencia; no se regenera CMake ni se
  cambia generador, flags, rutas, perfiles o versiones para facilitar pruebas.
- `nscbuilder3.py`, `nsc-builder`, Builder/controller y sus configuraciones
  quedan protegidos salvo autorización posterior explícita.
- CMake raíz, compiler-suite CMake, manifiestos no pertenecientes al ítem y
  configuración de compilación quedan protegidos.
- Las pruebas de Parte B usan el productor y target canónicos del catálogo. Un
  artefacto stale no demuestra PASS.

## 10. Contexto histórico del worktree original

Al crear la primera versión de este handover existían cambios ajenos que no se
editaron en aquel corte:

- `docs/compiler-optimization/test-infrastructure/test-execution-parity.csv`;
- `docs/compiler-optimization/test-infrastructure/test-execution-parity.json`;
- `docs/compiler-optimization/test-infrastructure/test-execution-parity.md`;
- `docs/DEVELOPMENT-COMMITS.md`, sin seguimiento;
- `docs/porting/`, sin seguimiento.

Esta lista conserva provenance histórica y no afirma que esos paths sigan
dirty. Antes de reanudar se debe consultar `git status`, atribuir cualquier
cambio actual y preservarlo; no se debe inferir estado vigente desde esta lista.

## 11. Protocolo de ramas

- El trabajo activo permanece siempre en `development`.
- Este handover crea `development-ChatGPT` desde el commit exacto del corte y
  vuelve inmediatamente a `development`.
- `development-ChatGPT` no reemplaza la frontera de versión ni autoriza mover
  una rama ya certificada.
- Sólo cuando Parte B alcance 147/147, T00–T17 estén cerradas y la batería final
  completa pase, se crea el commit exacto de cierre y la rama `v.3.0.4` desde
  ese commit, conforme al tracking. Después se continúa en `development`.
- No se crean, mueven ni reapuntan fronteras de versión anticipadamente.

## 12. Reanudación determinista

1. Cambiar a `development` sin alterar el worktree ajeno.
2. Leer completos los lineamientos, el plan 3.0.4, el tracking maestro y este
   handover.
3. Confirmar que el conteo continúa en 25/147 y que T03 está en 3/8.
4. Confirmar que `691661767` es ancestro de HEAD.
5. Congelar el alcance exclusivo de `NS304-T03-F04`.
6. Auditar sus productores y consumidores antes de editar.
7. Implementar y ejecutar únicamente la prueba puntual y guards causales de
   F04; actualizar tracking y expediente sólo con evidencia real.
8. Actualizar el conteo a 26/147 únicamente después del cierre secuencial de
   F04.

No se debe iniciar `NS304-T03-F05` antes del cierre verificable de F04.
