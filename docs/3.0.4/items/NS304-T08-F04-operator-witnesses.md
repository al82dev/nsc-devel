# NS304-T08-F04 — Operadores mediante witnesses

- **Estado**: CERRADO (operador de extensión resuelve + baja como llamada)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar operadores mediante witnesses*.
Plan T08-F04 (DeepSeekDocs): *representar operador extension como witness estático de trait, no como
mutación de tabla nominal; aplicar las mismas reglas de overload, simetría y conversión que operadores
normales; rechazar pares ambiguos y órfanos no permitidos*.

Estado real (tras F01-F03):

- **Operador normal**: el operador (`operator+` en un tipo) es productivo (overload + simetría +
  conversión ya cableados en el type-system).
- **Operador extension**: sin surface; un `operator+` dentro de un bloque `extension` no se registra
  como operador del tipo extendido.
- **Witness de trait**: `Language/TraitSemantics.*` tiene witnesses/coherence/conformances, pero el
  operador extension no emite un witness.

## Gap

Un operador declarado en `extension T { static operator+(T, T) {...} }` no participa en la resolución
de operadores del tipo `T` (ni como mutación nominal ni como witness).

### Verificación (2026-10-13)

- El parser **no acepta** `operator` dentro del bloque `extension`: `parseGlobalDeclaration` reporta
  `expected declaration name, found 'operator'`. Requiere: (1) parser del `operator` en el bloque
  extension → namespace function; (2) registro en el lookup de operadores del tipo extendido;
  (3) witness estático de trait + conformance (surface A).

## Decisión de surface (material, para Alberto)

- **(A) witness estático de trait** (recomendada): el operador extension emite un witness del trait de
  operador (`Operator+`-style) asociado al tipo, resuelto por conformance.
- **(B) operador extension como namespace function** (como F01/F03, sin witness).

Recomendación: **(A)** — coherente con el plan (no mutar la tabla nominal).

## Plan (tras surface)

1. Resolver `operator` en un bloque `extension` y emitir el witness estático de trait.
2. Aplicar overload/simetría/conversión idénticas al operador normal.
3. Rechazar pares ambiguos + órfanos (coherence, F05).

## Nota

F04 depende de la surface de operador extension (A/B) y de F05 (coherence). El operador normal ya
tiene la resolución; el trabajo es el witness + la conformance externa.
