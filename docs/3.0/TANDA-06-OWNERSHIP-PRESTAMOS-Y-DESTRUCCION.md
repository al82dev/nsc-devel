# Tanda 06 — Ownership, préstamos y destrucción / Ownership, borrows, and destruction

> **Estado 2026-10-03:** `IMPLEMENTING`; F01–F06 están cerrados de forma dirigida, el gate T06 completo sigue abierto.

## Resultado productivo actual

La auditoría del pipeline real mostró que NativeSharp 2.x ya tenía una base considerable conectada:

- propietarios locales RAII y parámetros `owned` con estado moved/maybe-moved;
- joins conservadores en branches y loops, y diagnósticos de use-after-move;
- préstamos locales con procedencia, alcance léxico e invalidación;
- rechazo de move/overwrite del propietario mientras vive un préstamo;
- regiones de cleanup y rutas de salida estructuradas;
- extracción de campos `owned` y elementos de array mediante load + null-store;
- destrucción automática de owners y campos todavía vivos.

La tanda operativa 13 cerró un hueco de exclusividad real: un movimiento parcial podía modificar
`owner.field` o `owner[index]` mientras un borrowed local del owner seguía vivo. El frontend ahora
recupera el propietario raíz de la expresión de storage, rechaza el conflicto para owner local o
parámetro `owned`, y conserva ese root en `MethodMoveBinding`/dump. La fixture c63 demuestra que el
campo extraído y el campo restante se destruyen exactamente una vez.

## Estado por feature

| ID | Estado productivo | Deuda que impide cerrar el ID completo |
|---|---|---|
| T06-F01 | **CERRADO**: tipos valor de edición 3 move-only por defecto, `copy` explícito, consumo con `move(...)`, joins CFG, arrays/genéricos y NSMETA 1.33 | Los moves parciales de fields/variants pertenecen expresamente a F02 |
| T06-F02 | **CERRADO**: estado moved/maybe-moved por ruta nominal, hermanos vivos, reinicialización, joins/back-edges, value-drop recursivo y destructor personalizado con guard optimizable | Payload activo de variants queda ligado al cierre de T03/F08; no debilita el contrato F02 |
| T06-F03 | **CERRADO**: place graph, reborrow mutable, suspensión del alias padre, split fields/índices constantes, slices half-open representables y fallback fail-closed | Lifetimes nombrados y procedencia cross-module pertenecen a F04/F07 |
| T06-F04 | **CERRADO**: lifetimes nombrados/inferidos, elisión determinista, `this`/`static`, outlives transitivo, equivalencia alfa de traits, cierre de escapes y NSMETA 1.34 cross-module | Los tipos función HRTB generales y captures prestados parametrizados pertenecen a T08; F04 cubre binder universal por método sin dejar escape inseguro |
| T06-F05 | **CERRADO**: `defer` edición 3 con capturas explícitas `ref`/`move`, registro léxico, orden LIFO y lowering al grafo único `finally`/RAII para salida normal y exits estructurados | El spelling y perfiles de panic pertenecen a T07; F05 ya comparte el cleanup de unwind y documenta que abort inmediato no ejecuta acciones posteriores |
| T06-F06 | **CERRADO**: `pin raii` y `pin ref`, places estáticos, placement autorreferencial, cleanup object-only, frontera conservadora de containers y bit pinned transportado por HIR/MIR para T09/T10 | Los state machines de `yield`/`async` pertenecen a T09/T10; F06 ya entrega la obligación que deben consumir, sin simular esas features |
| T06-F07 | Escape checks locales existentes; helper `escape-v1` aislado | Summary público/NSMETA, closures/globals/threads y cadena causal en tanda 14 |
| T06-F08 | Descriptor/drop entry del productor cruza NSMETA/DLL, validación fail-closed y witness O0 exactamente una vez | Glue genérico de arrays/variants y summaries de layout/destrucción indirecta |

## Invariantes de la vertical

- Un movimiento total invalida el owner nombrado y cualquier préstamo histórico derivado.
- Un movimiento parcial no consume el contenedor, pero sí modifica su storage; exige cero
  préstamos solapados. Un préstamo de un hermano demostrado disjunto no lo bloquea.
- El valor extraído obtiene ownership y la ranura fuente queda nula para que el cleanup posterior
  no produzca double drop.
- Un tipo por valor puede atravesar la misma sintaxis sin null-store de puntero.
- La disjunción se demuestra solo para fields nominales y storage array/pointer estructural con
  índices/slices constantes. Indexers, unions, offsets explícitos e índices dinámicos usan wildcard.
- El helper `OwnershipSemantics` sigue siendo oráculo/diseño; no sustituye al frontend productivo.

## Evidencia

- `nsc.ns3.t06.move-only-defaults`: 16/16 checks; ejecución O0/NSLink, locales, parámetros,
  returns, branches, loops, fields, fixed arrays, constraints, constructores, funciones libres,
  compatibilidad edición 2 y productor/consumidor NSMETA 1.33.
- `nsc.ns3.t06.partial-moves`: 7/7 checks; field movido con hermano vivo y reinicialización,
  double move, join, back-edge, restricción de destructor y destrucción exacta por scope, parámetro,
  early return y unwind nativo en O0/NSLink.
- `nsc.ns3.t06.reborrow-split`: 11/11 checks; fields hermanos, índices constantes, loop, calls,
  reborrow/suspensión, solapamientos, fallback dinámico/indexer, whole move y compatibilidad 2.x.
- `nsc.ns3.t06.lifetime-parameters`: 19/19 checks; sintaxis/edición, elisión, receiver, outlives,
  traits con renombrado alfa, rechazo de escapes por retorno/closure, NSMETA 1.34 y consumo separado
  O0 mediante NSLink.
- `nsc.ns3.t06.deterministic-cleanup`: 7/7 checks; LIFO en fallthrough/return/continue/break,
  captura move-only, destrucción única, grafo de cleanup y diagnósticos de captura/edición en
  O0 mediante NSLink.
- `nsc.ns3.t06.pinning`: 8/8 checks; `pin raii`, `pin ref`, placement autorreferencial,
  destructor object-only, MIR/caché y rechazos de relocation/escape/container dinámico en O0
  mediante NSLink.
- Matriz dirigida F02: 7/7 tests verdes incluyendo inicializadores, records, F01, ABI struct de
  edición 2, ownership Stage 50.11 y C6 struct members.
- Regresiones dirigidas: T04 constraints y T05 opaque/trait objects/extensions/operator traits,
  más contratos de migración/compatibilidad/versionado, todas verdes.

- `c63_ns3_move_borrow_drop`, O0, termina en 42.
- Method-body dump contiene `partial-owner-local=0` en el movimiento de `holder.Left`.
- MIR contiene `move-owned-field`, dos owners locales con cleanup y ruta de retorno `#3 -> #2`.
- Negativos dirigidos rechazan field desde owner local prestado, field desde parámetro `owned`
  prestado y elemento de array desde owner prestado con diagnóstico causal.

El gate T06 completo no se declara verde: todavía requiere F07-F08, interacción
con T07, matriz CFG/DLL exhaustiva, debugger y baseline final ejecutado por Alberto.

La tanda operativa 14 cerró una vertical de F06 y la frontera DLL de F08, pero no F07 ni el glue
genérico completo. El gate T06 sigue abierto por las deudas de ambas tandas operativas.
