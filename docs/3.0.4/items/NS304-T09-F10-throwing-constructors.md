# NS304-T09-F10 — Constructores `throws E`

- **Estado**: AUDITADO (efecto `throws` existe en callables; constructor `throws` sin surface)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar constructores `throws E`*.
Plan T09-F10 (DeepSeekDocs): *preservar efecto en AST/símbolo/tipo/overload/constraints/NSMETA;
exigir manejo/propagación en `new`/`this(...)`/`base(...)`/placement/in-place; MIR registra cada
base/campo construido y su cleanup inverso exacto; allocation failure/error tipado/unwind nativo son
rutas distintas; no publicar pointer/reference antes del éxito; emitir ABI/PDB + self-host*.

Estado real:

- **Efecto `throws`**: `Effect::Throws` + `declaredErrorTypeKey` ya existen para callables (T05-F07
  `future:<T>|<E>`). Un constructor **no** declara `throws E` hoy.
- **`new` con error**: sin manejo/propagación del error del constructor en el sitio `new`.

## Gap

Un constructor `throws E` no existe: no se preserva el error en el símbolo/overload, no se exige
manejo en `new`/`this`/`base`, y no hay cleanup inverso del objeto parcial al fallar (F09).

## Nota

F10 es el cierre de T09: habilita el constructor fallible (tipado, no excepción CLR/factory/Result) +
el cleanup parcial (F09). Depende de la surface `throws` en constructor (análogo al `throws` de
callable) y del drop de subobjetos ya existente.
