# Tanda 06 — Ownership, préstamos y destrucción / Ownership, borrows, and destruction

> **Estado 2026-09-29 después de la tanda operativa 13:** `IMPLEMENTING`.

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
| T06-F01 | Vertical integrada para locals RAII y parámetros `owned`; moved/maybe-moved y joins CFG | `Copy` explícito como contrato de tipos, defaults move-only generales, fields/arrays/generics completos |
| T06-F02 | Move parcial de campo owned y elemento con ranura nula como drop guard; owner raíz observable | Estado estático por campo, reinicialización, structs/variants nested y destructor personalizado |
| T06-F03 | Exclusividad conservadora del owner completo, incluidas proyecciones field/index | Reborrow mutable y préstamos divididos demostrablemente disjuntos para fields/índices/slices |
| T06-F04 | Lifetimes léxicos inferidos de borrowed locals y procedencia local/parámetro | Sintaxis nombrada, outlives/subtyping, returns prestados y metadata cross-module |
| T06-F05 | Destructor y cleanup LIFO existentes para owners locales y parámetros en salidas estructuradas | `defer`, panic/propagation T07, fallos parciales y destructor que falla |
| T06-F06 | Sólo helper histórico | Pinning productivo se aborda en tanda operativa 14 |
| T06-F07 | Escape checks locales existentes; helper `escape-v1` aislado | Summary público/NSMETA, closures/globals/threads y cadena causal en tanda 14 |
| T06-F08 | Destruction thunks y metadata 2.x existentes parcialmente | Drop glue genérico/indirecto y witness cross-DLL en tanda 14 |

## Invariantes de la vertical

- Un movimiento total invalida el owner nombrado y cualquier préstamo histórico derivado.
- Un movimiento parcial no consume el contenedor, pero sí modifica su storage; por eso exige cero
  préstamos activos del root owner.
- El valor extraído obtiene ownership y la ranura fuente queda nula para que el cleanup posterior
  no produzca double drop.
- Un tipo por valor puede atravesar la misma sintaxis sin null-store de puntero.
- El análisis es deliberadamente conservador: todavía no intenta demostrar que dos proyecciones son
  disjuntas. Esa precisión pertenece a T06-F03 completo, no debe simularse relajando el chequeo.
- El helper `OwnershipSemantics` sigue siendo oráculo/diseño; no sustituye al frontend productivo.

## Evidencia

- `c63_ns3_move_borrow_drop`, O0, termina en 42.
- Method-body dump contiene `partial-owner-local=0` en el movimiento de `holder.Left`.
- MIR contiene `move-owned-field`, dos owners locales con cleanup y ruta de retorno `#3 -> #2`.
- Negativos dirigidos rechazan field desde owner local prestado, field desde parámetro `owned`
  prestado y elemento de array desde owner prestado con diagnóstico causal.

El gate T06 completo no se declara verde: todavía requiere la tanda 14, interacción con T07,
metadata/DLL, matriz CFG exhaustiva, debugger y baseline final ejecutado por Alberto.
