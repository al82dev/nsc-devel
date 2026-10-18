# NSC 3.0.5 — R03C2C2E2: `match` statement en el coordinador compartido

## SCOPE LOCK

Esta hoja migra exclusivamente el statement `match` desde el fallback `Direct` al
`Parser::ParseContinuationCoordinator`. La expresión `match`, el análisis y lowering de patrones,
los patrones condicionales E3, los especiales F, Pratt, HIR, MIR, LLVM, sintaxis, ABI y NSMETA
permanecen protegidos.

El parser histórico no se elimina ni se reescribe porque todavía conserva consumidores no
migrados. El repositorio principal y ambos trackings quedan fuera del commit del trabajador.

## Contrato preservado

- El reconocimiento sigue siendo contextual mediante `startsParenthesizedBlock("match")`.
- El governing se analiza una vez; los textos exactos de `(`, `)`, `{` y `}` no cambian.
- Cada patrón se analiza una vez. Diagnósticos de binding no addressable, OR inválido, guard falso,
  catch-all e interval overlap conservan orden, rango y texto.
- El guard `when` conserva gramática conditional-expression; no se amplía a assignment-expression.
- El body sigue siendo un bloque obligatorio. La recuperación por body ausente crea el mismo bloque
  vacío, sincroniza statement, fija rango y consume la coma opcional.
- Catch-all e intervalos sólo publican cobertura global bajo guard ausente o constante true.
- Coverage conserva `match-coverage:v1:` y sus marcadores estables; los intervalos half-open
  permanecen ordenados/coalescidos con costo O(I log I).
- El temporal `$__nsc_match_<ordinal>` incrementa una vez, el selected flag empieza en false y el
  lowering conserva first-match, evaluación única, ownership const/ref/move, bindings antes del
  guard y selected=true antes del body original.
- No cambian AST público, Sema, HIR, MIR, ABI, NSMETA ni diagnósticos.

## Implementación

`StatementKind::Match` evita el fallback al coordinador histórico. `MatchFrame` y `MatchArm`,
almacenados en `std::deque`, conservan governing, arm pendiente, facts de pattern, arms terminados,
ownership, catch-all e índice de intervalos con posiciones estables.

Las continuaciones de governing, guard conditional-expression, body y cierre comparten la worklist
principal. El header de cada arm calcula sus facts una vez; el cierre reutiliza esos facts para
diagnósticos, coverage y lowering, sin rescans. Ninguna ruta E2 llama `parseMatchStatement`,
`parseStructuredControlStatement`, `parseBlock`, `parseStatement`, `parseExpression`, otro
`parse()` ni otro `drain()`.

Complejidad: O(tokens + arms + pattern facts + I log I), pila nativa O(1) en las aristas propias de
statement `match` y memoria heap proporcional a la profundidad activa.

## Prueba puntual e infraestructura

`TP-PAR-562` (`Host.Parser`, `CP-PAR-035`, `NIGHTLY`) contiene cuatro probes aislados bajo un único
selector:

1. 32,768 statement `match` anidados por cuerpos de arm, cursor EOF y stack nativo O(1);
2. governing único, temporales monotónicos, coverage, ownership, orden y lowering;
3. las cinco familias de diagnósticos de patterns, guard, catch-all e intervalos en orden exacto;
4. match vacío, body ausente y rechazo de assignment en guard conditional-expression.

La fuente queda registrada en CMake de compiler-suite, `TestRegistry.h`, registro default, design
CSV, allocation CSV y manifest JSONL; Builder 3 la descubre mediante `CanonicalCatalog`.

## Evidencia

- Validación estática de source-registration: PASS.
- `CanonicalCatalog`: PASS, 6,017 casos y TP-PAR-562 presente una vez.
- Límite de funciones, símbolos, columnas y `git diff --check`: PASS; la función E2 más
  extensa contiene 26 líneas ejecutables según el conteo estático local.
- Build exclusivo de `nsc_component_tests`: PASS, 290 pasos, exit 0.
- Única prueba seleccionada `--case TP-PAR-562`: PASS, exit 0, 650,528 microsegundos; pasaron
  los cuatro probes, incluido el contrafactual de profundidad 32,768.
- La primera corrida focal reveló dos defectos del oráculo: contaba cinco referencias al temporal
  por statement en vez del único nodo con coverage, y suponía que un list pattern no consume su
  ordinal interno documentado. Se corrigió sólo el fixture y se repitió el mismo target/caso; el
  producto, coverage, ownership, evaluación única y diagnósticos ya coincidían con el contrato.

Logs persistentes:

- `A:\NSC-devel-paralelo\logs\agente-2\r03c2c2e2-build-nsc-component-tests.log`
- `A:\NSC-devel-paralelo\logs\agente-2\r03c2c2e2-build-nsc-component-tests-final.log`
- `A:\NSC-devel-paralelo\logs\agente-2\r03c2c2e2-tp-par-562-final.log`

No se ejecutarán Host.Parser completo, CTest, TL-LANG, regresiones, minisuite, baseline, deploy ni
instalación.
