# NS304-T03-F07 — Bajar a length checks, branches y borrows sin colección ni boxing

- **Estado**: EN VERIFICACIÓN (mayormente satisfecho por F01–F06)
- **Rama**: development

## Auditoría (línea base)

El lowering de list pattern ya baja a código eficiente sin colección ni boxing:

1. **Length check**: `makePatternCondition` (List) emite `Length >= fixedCount` / `== fixedCount`
   con `Length` marcado `list-pattern-length` (resuelto a `.Length` miembro o `Length()` auto-invocado).
2. **Element tests**: `source[index]` directo (prefijo) o `source[Length - fromEnd]` (sufijo) vía
   `makeIndexExpression` — sin materializar una colección intermedia.
3. **Rest capture**: `source.Slice(start, Length - fixedCount)` marcado `list-pattern-rest`, que
   sintetiza `Span<T>`/`ReadOnlySpan<T>`/`StringView` SIN copia (F03). Los binding modes `copy`/`move`
   son por elemento (F06), no un boxing del scrutinee completo.

## Verificación requerida

- `TL-LANG-310` ya cubre array/string rest sin copia (view).
- Falta una prueba puntual que demuestre que el path NO asigna: un list pattern sobre un array
  grande debe compilar sin `new`/`stackalloc` implícito en el camino del rest.

## Decisión

F07 se considera satisfecho por el lowering ya implementado; se cierra con una prueba de
regresión de "sin asignación implícita" en `TL-LANG-314` (o se marca como cubierto por 308–310
si el análisis de IR lo confirma).
