# NSC 3.0.5 — R03C2C2E3: conditional-pattern en el coordinador compartido

## SCOPE LOCK

Esta hoja migra exclusivamente `if (let ...)`, `while (let ...)` y `let ... else` desde el
fallback `Direct` al `Parser::ParseContinuationCoordinator`. Ordinary if/while, switch, match,
patterns, las formas especiales R03C2C2F, Pratt, HIR, MIR, LLVM, sintaxis, ABI y NSMETA permanecen
protegidos.

El parser histórico se conserva porque todavía tiene consumidores no migrados. El repositorio
principal y ambos trackings quedan fuera del commit del trabajador.

## Contrato preservado

- El reconocimiento sigue limitado a `if`/`while` seguidos por `(let ...)` y al contextual `let`.
- Pattern usa `parsePattern(false)` y la fuente conserva assignment-expression mediante el work
  item `Expression`; no se abre una gramática distinta.
- El análisis de pattern se ejecuta una vez y conserva orden, rango y texto de los diagnósticos de
  binding no addressable y OR-pattern incompatible.
- `ref`, `ref mut` y `move` conservan el ownership del temporal; sólo value/copy produce temporal
  const y las referencias mantienen `local:ref`.
- `if-let` y `while-let` aceptan embedded statements. `else if` conserva nearest-if sin wrapper
  adicional. `let-else` exige bloque divergente y punto y coma con los textos históricos.
- La fuente se evalúa una vez por selección y una vez por iteración de `while-let`.
- Los temporales conservan `$__nsc_conditional_pattern_<ordinal>` y `_matched`, compartiendo el
  ordinal monotónico con match.
- Prelude, bindings, success/failure, `while(true)`, break y marker
  `conditional-pattern-inline` mantienen forma y orden observables.
- No cambian AST público, Sema, HIR, MIR, ABI, NSMETA ni diagnósticos.

## Implementación

`StatementKind::ConditionalPattern` clasifica las tres formas antes de ordinary if/while y del
fallback. `ConditionalPatternFrame`, almacenado en `std::deque`, conserva prefix, pattern, source,
bodies y ownership con índices estables.

Cinco continuaciones completan source, success, failure, while y let-else sobre la worklist
compartida. Source usa `Expression`; success/failure usan `Embedded`, excepto `else if`, que agenda
`Statement` directamente. Let-else agenda `Block` y completa el slot aun en recovery por `{`
ausente. Los helpers de lowering reproducen la implementación histórica sin reanalizar patterns.

La ruta válida E3 no llama `parseConditionalPatternStatement`, `parseStructuredControlStatement`,
`parseStatement`, `parseBlock`, otro `parse()` ni otro `drain()`.

Complejidad: O(tokens + pattern facts + lowering), pila nativa O(1) en las aristas propias de las
tres formas y memoria heap proporcional a la profundidad activa.

## Prueba puntual e infraestructura

`TP-PAR-563` (`Host.Parser`, `CP-PAR-035`, `NIGHTLY`) contiene cinco probes aislados bajo un único
selector:

1. 32,768 `if-let` anidados por embedded bodies;
2. 32,768 `while-let` anidados, con loop y break por nivel;
3. 32,768 `let-else` anidados, con divergencia válida y flattening inline;
4. temporales compartidos/monotónicos con match, ownership, const/ref y evaluación única;
5. orden/texto de diagnósticos y recuperación de `=`, divergencia y `;` con statement posterior.

La fuente queda registrada en CMake de compiler-suite, `TestRegistry.h`, registro default, design
CSV, allocation CSV y manifest JSONL; Builder 3 la descubre mediante `CanonicalCatalog`.

## Evidencia

- Validación estática de source-registration: PASS.
- `CanonicalCatalog`: PASS, 6,018 casos y TP-PAR-563 presente una vez.
- Scan de fachadas/drains prohibidos y `git diff --check`: PASS.
- Build exclusivo de `nsc_component_tests`: PASS, 290 pasos, exit 0.
- Única prueba seleccionada `--case TP-PAR-563`: PASS, exit 0, 1,797,013 microsegundos; pasaron
  los cinco probes, incluidos los tres contrafactuales de profundidad 32,768.

Logs persistentes:

- `A:\NSC-devel-paralelo\logs\agente-2\r03c2c2e3-configure.log`
- `A:\NSC-devel-paralelo\logs\agente-2\r03c2c2e3-build-nsc-component-tests.log`
- `A:\NSC-devel-paralelo\logs\agente-2\r03c2c2e3-tp-par-563.log`

No se ejecutaron Host.Parser completo, CTest, TL-LANG, regresiones, minisuite, baseline, deploy ni
instalación.
