# NS304-T09-F01 — Miembros `required` y definite initialization

- **Estado**: CERRADO (parser + flag + validación en constructor)
- **Rama**: development

## Resultado

- **Parser** (`src/Parser/Parser.cpp`): `required` se reconoce como modificador contextual en la
  lista de modificadores de miembro de clase; el campo se marca con el sufijo `required` en su
  `auxiliaryText`.
- **Member model** (`ProjectMemberModel.h/.cpp`): flag `ClassMemberSymbol::isRequired`.
- **Sema** (`ProjectMethodBodyModel`): el estado activo de `required` forma parte de
  `OwnershipFlowSnapshot`; los caminos alcanzables se unen por intersección. Cada `return` normal
  y el fallthrough del constructor participan en la intersección final; `throw`/`never` no crean
  una instancia observable. `assignedRequiredFields_` conserva únicamente el resultado final por
  constructor y el merger de shards propaga esos resultados.
- Prueba `TL-LANG-335`: asignación completa → 42; ausencia total y asignación en una sola rama →
  rechazo; ambas ramas → aceptación; rama que lanza no contamina el join; `return` anticipado sin
  asignación → rechazo.

## Residual

- **Object initializer** (`new T { X = 1 }`): el initializer de agregado inicializa campos, no
  properties; no aplica al enforcement de `required` (capacidad aparte).
