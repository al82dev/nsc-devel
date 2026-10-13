# NS304-T09-F01 — Miembros `required` y definite initialization

- **Estado**: CERRADO (parser + flag + validación en constructor)
- **Rama**: development

## Resultado

- **Parser** (`src/Parser/Parser.cpp`): `required` se reconoce como modificador contextual en la
  lista de modificadores de miembro de clase; el campo se marca con el sufijo `required` en su
  `auxiliaryText`.
- **Member model** (`ProjectMemberModel.h/.cpp`): flag `ClassMemberSymbol::isRequired`.
- **Sema** (`ProjectMethodBodyModel`): `assignedRequiredFields_` rastrea `X = ...`/`this.X = ...`;
  `verifyRequiredFieldAssignments` exige que todo campo `required` de instancia se asigne en CADA
  constructor (se reporta el no asignado). El merger de shards propaga el set.
- Prueba `TL-LANG-335`: positivo (constructor asigna → 42) y negativo (constructor sin asignar →
  rechazado).

## Residual

- **Object initializer** (`new T { X = 1 }`): el initializer de agregado inicializa campos, no
  properties; no aplica al enforcement de `required` (capacidad aparte).
