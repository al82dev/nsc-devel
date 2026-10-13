# NS304-T03-F07 — Bajar a length checks, branches y borrows sin colección ni boxing

- **Estado**: ABIERTO (implementación parcial; falta evidencia de coste)
- **Rama**: development

## Auditoría (línea base)

El código actual contiene las siguientes rutas de lowering, compatibles con el objetivo de F07,
pero todavía sin un oráculo que demuestre ausencia general de colección, boxing o temporales:

1. **Length check**: `makePatternCondition` (List) emite `Length >= fixedCount` / `== fixedCount`
   con `Length` marcado `list-pattern-length` (resuelto a `.Length` miembro o `Length()` auto-invocado).
2. **Element tests**: `source[index]` directo (prefijo) o `source[Length - fromEnd]` (sufijo) vía
   `makeIndexExpression` — sin materializar una colección intermedia.
3. **Rest capture**: `source.Slice(start, Length - fixedCount)` marcado `list-pattern-rest`, que
   sintetiza `Span<T>`/`ReadOnlySpan<T>`/`StringView` SIN copia (F03). Los binding modes `copy`/`move`
   son por elemento (F06), no un boxing del scrutinee completo.

## Evidencia disponible y límite

- F01–F06 demuestran forma, checks, indexación, captura rest y bindings.
- `TL-LANG-310` cubre únicamente el sustrato `string`; no demuestra el lowering general ni la
  ausencia de colección, boxing o asignación temporal para los demás sustratos.
- Falta una prueba puntual que demuestre que el path NO asigna: un list pattern sobre un array
  grande debe compilar sin `new`/`stackalloc` implícito en el camino del rest, acompañado por
  evidencia de IR o un oráculo equivalente que observe ese contrato.

## Decisión

F07 permanece abierto. El lowering existente es la base, pero ningún caso actual certifica la
ausencia general de colección, boxing o asignación temporal. Se cerrará únicamente con una
prueba de regresión específica y evidencia observable del IR/coste; `TL-LANG-308..310` no son
por sí solos evidencia suficiente.
