# NS3-T12-F05 — Disponibilidad normativa / Normative availability

## Español

NativeSharp 3 reconoce `deprecated`, `since`, `removed`, `target`, `architecture` y `allow`
como atributos reservados del lenguaje. No requieren una definición escrita por el usuario y no
pueden ser reemplazados silenciosamente por otra semántica.

Los contratos se enlazan al ordinal canónico del tipo o miembro, después de resolver nombres y
sobrecargas. `since` y `removed` se comparan contra la edición del proyecto consumidor; el target
productivo actual es `windows` y la arquitectura productiva es `x64`. Una incompatibilidad es un
error en el sitio de uso. `deprecated` produce una advertencia estructurada que conserva mensaje y
reemplazo; `#[allow("deprecated")]` en el tipo consumidor la suprime de forma acotada.

La validación recorre usos de tipos —incluidos argumentos genéricos y expansiones ya materializadas—
y llamadas resueltas a miembros. No hace resolución de paquetes ni versiones: eso queda fuera de
este contrato de lenguaje.

## English

NativeSharp 3 recognizes `deprecated`, `since`, `removed`, `target`, `architecture`, and `allow`
as language-owned attributes. They require no source-defined attribute declaration and cannot be
silently replaced with unrelated semantics.

Contracts bind to the canonical type or member ordinal after name and overload resolution.
`since` and `removed` are evaluated against the consuming project's language edition; the current
production target is `windows` and the production architecture is `x64`. An incompatible use is
an error at the use site. `deprecated` emits a structured warning retaining its message and
replacement; `#[allow("deprecated")]` on the consuming type suppresses it locally.

Validation covers type uses—including generic arguments and already-materialized macro output—and
resolved member calls. It deliberately performs no package or version solving.
