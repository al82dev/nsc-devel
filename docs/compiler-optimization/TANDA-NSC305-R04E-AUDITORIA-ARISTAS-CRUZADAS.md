# NSC 3.0.5 — R04E: auditoría final de aristas cruzadas de statements

## SCOPE LOCK

- **Repositorio/rama/revisión base:** `A:\nsc-devel`, `development`, `9dc2ddebb`.
- **Objetivo exacto:** demostrar, clasificar y cerrar las reentradas que sobreviven al alternar
  familias de statements, sin confundir pruebas same-family con un parser globalmente iterativo.
- **Naturaleza inicial:** auditoría causal con contrafactuales directos; cualquier cambio de
  producto exige un subítem y una evidencia roja independiente.
- **Archivos de evidencia autorizados:** `tests/unit/ProjectMethodBodyModelTests.cpp`, este
  expediente y tracking externo.
- **Producto inicialmente protegido:** todo el compilador. Sólo una reproducción roja autoriza
  abrir un subítem acotado en `src/Parser/Parser.cpp`.
- **Familias obligatorias:** controles ordinarios, `foreach`, condition-pattern,
  `switch`/`match`, `try`/`using`/`defer` y bloques.
- **Invariantes:** sintaxis, AST, ranges, diagnostics, recovery, cleanup, temporales, semántica,
  API, ABI, NSMETA, CLI y artefactos 3.0.4.
- **Separación obligatoria:** un overflow que ocurra después de syntax-parse, por ejemplo en
  `members`, no se atribuye al parser y se rastrea como defecto independiente.
- **Prohibido:** aumentar stack, reducir profundidad, guards como solución, baseline, suite
  completa, deploy, instalación, push, pull o fetch.
- **Criterio de terminación:** todas las aristas cruzadas quedan verdes o transferidas con
  evidencia a un subítem explícito; ninguna familia se declara cerrada por inferencia.

## Matriz inicial de reentrada

| Productor con estado pendiente | Hijo que puede reentrar | Riesgo |
|---|---|---|
| `StructuredControlParser` | `parseForeachStatement` | máquinas separadas se llaman por bloque |
| `parseForeachStatement` | control estructurado | reentrada inversa por `parseStatement` |
| control estructurado/foreach | condition-pattern | parser monolítico conserva bodies |
| condition-pattern | control/foreach/condition-pattern | embedded block y `else if (let ...)` |
| cualquier bloque | cleanup/exceptions | migrado a la máquina estructural en R04D |
| switch/match | statements de sección/brazo | migrado parcialmente; se audita mezcla |

## Estado

En curso.

## R04E1 — ordinary control / `foreach`

### Evidencia roja

Una unidad con 1,024 cuerpos alternados `if (true) { ... }` / `foreach (...) { ... }` terminó
contra `9dc2ddebb` con `0xC00000FD` (`-1073741571`). La prueba aisló la reentrada mutua entre
`StructuredControlParser` y la máquina local de `parseForeachStatement`.

### Cierre

`foreach` se integró a la misma pila `work_` mediante `ParseForeach`/`FinishForeach`. Se reutilizan
sin cambios `parseForeachHeader` y `lowerForeachStatement`; el wrapper público delega a la máquina
estructural. La prueba alternada pasa dentro de `PASS: 106 checks` y `nsc` conserva su contrato de
host runtime estático.

R04E permanece activo para condition-pattern y para la clasificación separada del overflow en
`members`.
