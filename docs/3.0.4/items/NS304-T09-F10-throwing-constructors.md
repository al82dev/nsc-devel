# NS304-T09-F10 — Constructores `throws E`

- **Estado**: PARCIAL (slice 1: constructor `throws E` parsea + error en símbolo; slice 2 exigir
  manejo en `new`/`this`/`base` y slice 3 cleanup parcial F09 pendientes)
- **Rama**: development

## Resultado (slice 1 — 2026-10-14)

- `Parser::parseConstructorTail` ahora llama `parseThrowsClause(constructor)`, así el constructor
  declara `throws E` como cualquier callable. `ProjectMemberModel` ya leía `throwsTypeSyntax()`
  (declaredErrorTypeKey), así que el error queda en el símbolo y se valida (debe ser
  `NativeSharp.Core.Exception` o derivar).
- Prueba puntual **TL-LANG-407** (exit 0) + guard **TP-PAR-261** (contrato actualizado: throws
  aceptado y preservado). El mecanismo completo funciona: `public C() throws E { throw new E(); }`
  + `try { new C(); } catch (E) { ... }` propaga y captura el error por `new`.

## Residual (slices 2-3)

- **Exigir manejo en `new`/`this`/`base`**: hoy el sitio `new` no obliga a manejar/propagar el error
  declarado (idéntico al estado de los métodos `throws`). Es el enforcement del modelo tipado
  completo, acoplado a T06-F07 (error tipado, pendiente de Alberto).
- **Cleanup parcial (F09)**: drop inverso exacto del objeto parcial al fallar el constructor.

## Auditoría (línea base)

Contrato (tracking): *Implementar constructores `throws E`*.
Plan T09-F10 (DeepSeekDocs): *preservar efecto en AST/símbolo/tipo/overload/constraints/NSMETA;
exigir manejo/propagación en `new`/`this(...)`/`base(...)`/placement/in-place; MIR registra cada
base/campo construido y su cleanup inverso exacto; allocation failure/error tipado/unwind nativo son
rutas distintas; no publicar pointer/reference antes del éxito; emitir ABI/PDB + self-host*.

Estado real:

- **Efecto `throws`**: `Effect::Throws` + `declaredErrorTypeKey` ya existen para callables (T05-F07
  `future:<T>|<E>`). El constructor ahora declara `throws E` (slice 1).
- **`new` con error**: el mecanismo de propagación/captura funciona (slice 1); falta exigirlo.

## Gap

Residual: no se exige manejo en `new`/`this`/`base` y no hay cleanup inverso del objeto parcial al
fallar (F09). El slice 1 (símbolo/overload) quedó cerrado.

## Nota

F10 es el cierre de T09: habilita el constructor fallible (tipado, no excepción CLR/factory/Result) +
el cleanup parcial (F09). Depende de la surface `throws` en constructor (análogo al `throws` de
callable) y del drop de subobjetos ya existente.
