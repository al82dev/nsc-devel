# NS304-T06-F02 — AST y semántica de contexto de generator

- **Estado**: PARCIAL (surface `Iterator<T>` + contexto yield resueltos; marcado Effect::Generator + unificar item + rechazo return/yield pendientes)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar AST, semántica y restricciones de contexto de generator*.
Plan T06-F02 (DeepSeekDocs): *marcar callable como generator por semántica (no búsqueda textual);
inferir tipo de item o validar el declarado; unificar todos los yields; prohibir mezcla incompatible
de retorno ordinario y yield*.

Estado real:

- **AST** (T06-F01, cerrado): `YieldReturnStatement`/`YieldBreakStatement` existen (`AstNodeKind`).
  Parser produce ambos (`Parser.cpp:9229/9236`).
- **Semántica**: el único consumidor es un rechazo puntual en `ProjectMethodBodyModel.cpp:5221-5228`
  (`yield requires the generator state machine lowering (NS304-T06-F03)`). No hay marcado de
  generator, ni inferencia de item, ni validación de retorno+yield.
- **Modelo disponible pero NO cableado**: `IterationSemantics.h` define `SynchronousGeneratorPlan`,
  `buildSynchronousGeneratorPlan`, `GeneratorSuspension`, `generatorResumeState`, etc.
  (frame/state machine). Igual que F03 documentó para el plan builder async: unit-tested, sin
  consumidor productivo.
- **Efectos**: `sema::Effect` no tiene `Generator` (sólo Unsafe/Throws/Async/Comptime/NoHeap).
- **Iterador/foreach**: no hay tipo `Iterator<T>` resoluble ni protocolo `foreach` productivo (eso
  es F08). La surface `comprehend(...) { yield return ...; }` es de T09 (comprehensions), distinta
  del generator-method de T06.

## Gap

El callable con `yield return` no se marca como generator ni se tipa el item; el yield siempre
rechaza con el diagnóstico de lowering. No hay superficie resoluble para declarar un generator-method
(ni `Iterator<T>` ni inferencia de item), y no se distingue `yield return` en un método común (error
de contexto) del `yield return` legítimo de un generator.

## Decisión de surface (material, para Alberto)

El generator-method necesita una **declaración** para que el Sema lo marque antes del lowering. Dos
opciones:

- **(A) Return type `Iterator<T>`**: `Iterator<int> Gen() { yield return 1; }`. `Iterator<T>` es
  resoluble (map a un iterator-type opaco, como `Future<T>`→`future:<T>` en F09). El item T es el
  argumento; los yields se validan contra T.
- **(B) Inferencia total**: sin tipo de retorno declarado (`Gen() { yield return 1; }`); el Sema
  infiere el item del primer yield. Más ergonómico, pero exige una regla de inferencia + un
  iterator-type sintetizado sin nombre.

Recomendación: **(A)** — consistente con F09 (`Future<T>`), da un item-type declarado estable y evita
la inferencia implícita antes de congelar el ABI (F08).

## Plan (tras fijar la surface)

1. Añadir `sema::Effect::Generator` (o flag equivalente) y marcar el callable con `yield return`.
2. Resolver `Iterator<T>` (opción A) o inferir el item (opción B); unificar todos los yields a un
   item-type.
3. Rechazar `return v;` (retorno ordinario) en un generator y `yield return` fuera de él (contexto).
4. Mantener el rechazo de lowering (F03) para el generator YA marcado, sin confundirlo con el error
   de contexto.

## Prueba puntual (pendiente de surface)

`Iterator<int> Gen() { yield return 1; yield return 2; }` debe marcar generator + item `builtin:int`
sin diagnosticar; `int M() { yield return 1; }` debe diagnosticar contexto; `Iterator<int> G() {
yield return 1; return 2; }` debe rechazar el return ordinario.

## Nota

F02 es la base semántica; el state machine productivo es F03. El item-type y el marcado son el hueco
demostrable de F02 sin tocar el lowering.

### Cierre parcial (surface + contexto) — verificado 2026-10-13

- `IterationSemantics`: `iteratorTypeKey(T)` → `iterator:<T>` + `iteratorInnerTypeKey` inverso.
- `Effect::Generator` añadido (enum + `effectName`).
- `Iterator<T>` es surface estructural (parser known-type, `canonicalTypeKey`, generic model, name
  lookup) — mismo patrón que `Future<T>`.
- El handling de `yield` distingue: return type `Iterator<T>` → diagnostic F03 (`generator state
  machine lowering`); callable ordinario → diagnostic de contexto (`yield requires a generator`).
- Verificado: `int M(){yield return 1;}` → contexto; `Iterator<int> Gen(){yield return 42;}` → F03.
  Registrado `TL-LANG-379` (PASS) + `TL-LANG-329` actualizado. Commit `e8eca146`.

Queda: **marcar** el callable con `Effect::Generator` (el flag está, no se setea), **unificar** todos
los yields a un item-type y **rechazar** `return v;` en un generator.
