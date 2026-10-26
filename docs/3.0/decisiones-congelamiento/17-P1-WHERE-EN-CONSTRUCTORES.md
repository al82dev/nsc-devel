# Congelamiento 3.0 P1 (DEC-12) — `where` en constructores de tipos genéricos

Fecha: 2026-10-26. Rama: `development-release-candidate`. Revisión de partida: `62ad32baa`.

## Propósito y problema

`62ad32baa` extendió `where` a los métodos de tipos genéricos. P2 necesita lo mismo en constructores:
`SortedList`, `SortedSet`, `SortedDictionary` y `PriorityQueue` reciben un `Comparison<T>` por valor en su
constructor, y con P2 ese constructor debe exigir `T : copy`. El parser no aceptaba `where` en un constructor, y
el binding de construcciones no consultaba el modelo de constraints.

Decisión de Alberto Huerta Luna (P1): extender `where` a los miembros de tipos genéricos. Un constructor es un
miembro.

## SCOPE LOCK

- **Objetivo.** Un constructor de una clase o struct genérica acepta `where` sobre un parámetro de su tipo. Una
  construcción que elige un constructor cuya cláusula falla se rechaza en el `new`.
- **Naturaleza.** Completa P1.
- **Debe quedar idéntico.**
  - Constructores sin cláusula y todo lo demás de `62ad32baa`.
  - La elección de constructor por tipos.
  - Las cadenas `this(...)`/`base(...)`.
- **Modificable.**
  - Parser: `MemberParser.cpp` (`parseConstructorTail`), `GenericSyntaxParser.cpp`.
  - Constraints: `ProjectGenericConstraintModel.cpp`.
  - Construcciones: `ConstructorCandidateBinding.h`, `ProjectConstructionModel.h`, `ConstructionBinding.cpp`.
  - `ProjectMethodBodyModel.h`: un getter.
  - `GenericConstraintClause.h`: comentario.
  - `Freeze30ContractTests.cpp`.
- **Prohibido.**
  - Validar en la especialización.
  - Cambiar la resolución de sobrecargas por tipos.
- **Terminación.** Sondas Y1–Y6 en `-O0`; P1 sin cambios.

## Contrato

1. **Parser.** `parseConstructorTail` lee las cláusulas `where` después de `throws` y antes del inicializador
   `: this(...)`/`: base(...)`. Se aplica la misma regla que en los métodos: solo un parámetro del tipo que
   contiene al constructor.
2. **Validación.** `validateSpecializationMemberClauses` también recorre los constructores. El mensaje los
   nombra «'Holder constructor'», porque un constructor especializado lleva el nombre mangleado del tipo.
3. **Construcción.**
   - `evaluateConstructionCandidates` descarta un candidato viable cuya cláusula falla y lo marca
     `constraintRejected`.
   - Si no queda ningún candidato viable, `resolveConstructionCandidates` emite el diagnóstico diferido en el
     `new`.
   - Si otro candidato es viable por tipos, se elige ese.
   - El modelo de constraints llega por `ProjectMethodBodyModel::genericConstraintModel()`.

## Validación

nsc Release recompilado (exit 0). Sondas en `A:\temp\nsc-dec\d13`:

| Sonda | Forma | Resultado |
|---|---|---|
| Y1CtorAccept | `Holder<int>(Cmp)`, `Holder<Guard>(InCmp)`, `Holder<Guard>()` | 123 |
| Y2CtorReject | `new Holder<Guard>(Cmp<Guard>)` | «generic parameter 'T' of 'Holder constructor' requires copyable storage…» en el `new` |
| Y4CtorNonGeneric | `where T : copy` en el constructor de una clase no genérica | «'T' is not a generic…» |
| Y5StructCtor | `new Cell<Guard>(5)` con `Cell(int) where T : copy` | rechazada en el `new` |
| Y6CtorTest | fuente de `P1ConstructorAccepted` | 1234 |

- Sondas de P1 (W1–W9 en `-O2`, biblioteca precompilada WLibOk/WLibBad): sin cambios.
- Sondas de DEC-11 con `List` y `LinkedList`: sin cambios.

## Límite observado

Una lambda con parámetros `in` como argumento directo de un `new` no compila: «an unconverted lambda parameter
must be passed by value; ref/out/in require a target function type». Los argumentos de un constructor se
analizan sin tipo destino. Con una local del tipo delegado sí funciona (Y1). No es parte de este cambio.

## No verificado

- `Freeze30ContractTests` no está configurado en los árboles locales; `testConstructorCopyConstraint` equivale a
  las sondas Y.
- Batería completa: queda para el cierre final.
