# NS304-T11-F07 — Ref fields, scoped y summaries

- **Estado**: IMPLEMENTADO (ref fields con escape safety de origen de storage; scoped/ref readonly/summary residuales)
- **Rama**: development

## Resultado (ref fields + escape safety — 2026-10-14)

- **Parser**: `ref T Field` (campo referencia) y `target = ref storage` (asignación por referencia).
- **MemberModel**: `isReferenceReturn` marca también campos `ref T` (no solo métodos).
- **Sema** (`analyzeAssignment`): un `field = ref storage` registra el `MethodEscapeOrigin` del storage
  por ordinal del campo (`refFieldOrigins_`).
- **Escape evaluator** (`startMember`): el acceso a un campo ref devuelve el origen registrado del
  storage, no el receiver struct (que se copia al retornar y ocultaría el dangling).
- **Return** (`analyzeReturnStatement`): retornar un struct valor cuyo ref field aliasa storage del
  frame actual se rechaza (`cannot return a value whose ref field 'F' aliases storage owned by the
  current stack frame`); el alias a un parámetro `ref` (storage externo) se permite.
- Prueba: `TL-LANG-475` — store/read de ref field (exit 5) + rechazo del escape → **PASS**.

## Residual (fuera del surface)

- **`scoped`**: la anotación de lifetime acotado no está modelada.
- **`ref readonly`**: el campo referencia inmutable (`ref readonly T`) no está modelado.
- **Summary intermodular**: NSMETA no publica aún orígenes de retorno/`scoped` de ref fields.

## Auditoría (línea base)

Contrato (tracking): *Implementar ref fields, scoped y summaries*.
Plan T11-F07 (DeepSeekDocs): *completar definite assignment, mutabilidad, reborrow y escape de ref
fields; publicar summary intermodular suficiente*.

## Commits

- `d5264d8d` — ref fields con escape safety de origen de storage (TL-LANG-475).
