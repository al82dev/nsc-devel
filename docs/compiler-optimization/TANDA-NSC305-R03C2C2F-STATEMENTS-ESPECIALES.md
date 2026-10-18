# NSC 3.0.5 — R03C2C2F: statements especiales en el coordinador compartido

## SCOPE LOCK

Esta hoja migra exclusivamente `fp(...) {}`, `unsafe [...] {}`, inline `asm` y `yield` dentro de
una comprehension desde el fallback `Direct` al `Parser::ParseContinuationCoordinator`. También
convierte a worklist la propagación de modo FP que formaba parte de la misma ruta profunda.

Se conservan sintaxis, AST, cursor, rangos, diagnósticos, recovery, metadata asm, evaluación,
ABI y NSMETA. R03C3–C5, R03A/B, HIR, MIR, LLVM, el repositorio principal y ambos trackings quedan
protegidos. El único target autorizado es `nsc_component_tests` y la única prueba ejecutable es
`TP-PAR-564`.

## Implementación

- `StatementKind::Direct` desaparece. Las cuatro familias tienen clasificación y transición
  explícitas.
- Los scopes publican su block mediante `ResultSlot`; `fp` aplica su modo al terminar y `unsafe`
  conserva razón y warning de amplitud.
- La propagación FP usa una pila heap explícita con el mismo preorden, prioridad first/second/third
  y children crecientes, además de la misma poda ante un modo no `Inherit`.
- Inline asm conserva header, cuerpo exacto, metadata y validación de control antes de procesar
  `out`, `in` y `clobber`. Los outputs usan frontera unary; los inputs usan la continuación de
  assignment-expression; ordinals, constraints y placeholders se ensamblan en el orden histórico.
- Comprehension-yield copia `builderName` y `loopLabel` antes de agendar el valor. `yield break`,
  `yield expr` y `yield return expr` conservan su forma y textos auxiliares.
- Ninguna transición statement/block de F llama `parseStatementImpl`, `parseStatement`,
  `parseBlock`, `parseStructuredControlStatement`, `parseWithContinuation`, `parse` ni `drain`.
  El output asm conserva deliberadamente la frontera histórica `parseUnaryExpression`; no se
  ensanchó a assignment-expression. La migración general de unary/postfix/primary sigue siendo
  responsabilidad contable de R03C4/R03C5 y no se atribuye a esta hoja.

Complejidad propia: O(tokens + nodos), heap O(profundidad activa) y stack nativo O(1) para las
aristas statement/block de F y la propagación FP. Este cierre no afirma que las hojas posteriores
del parser ya sean iterativas.

## Prueba puntual e infraestructura

`TP-PAR-564` (`Host.Parser`, `CP-PAR-036`, `NIGHTLY`) contiene cinco probes aislados bajo un único
selector:

1. 32,768 niveles mixtos con scope FP exterior, unsafe anidados, asm y comprehension-yield;
2. propagación/poda FP y warning/razón unsafe;
3. metadata asm, output unary, inputs assignment y lambda-block, constraints, clobbers y cursor;
4. formas y nombres copiados de los tres comprehension-yield;
5. diagnósticos y recuperación de scopes y delimitador asm con statement posterior.

La fuente se registra en CMake de compiler-suite, `TestRegistry.h`, registro default, design CSV,
allocation CSV y manifest JSONL. Builder 3 y `CanonicalCatalog` la descubren por el manifiesto.

## Evidencia

- `validate_source_registration.py`: PASS, sin fuentes inesperadas ni registros ausentes.
- `CanonicalCatalog.load()`: PASS; 6,019 casos, 30 componentes y 682 contratos.
- Build exclusivo de `nsc_component_tests`: PASS, 108/108 pasos; log
  `A:\NSC-devel-paralelo\logs\agente-2\r03c2c2f-build-nsc-component-tests.log`.
- Única prueba ejecutada: `nsc_component_tests.exe --case TP-PAR-564`: PASS, exit 0,
  301,742 us; log `A:\NSC-devel-paralelo\logs\agente-2\r03c2c2f-tp-par-564.log`.
- Los cinco probes del mismo caso pasan, incluido el contrafactual de 32,768 niveles. Durante el
  cierre se corrigieron dos expectativas del fixture, no el producto: la razón unsafe conserva el
  lexema histórico con comillas y el asm usa el placeholder ordinal `$0` para un input cuyo nodo
  raíz es una asignación.
- Auditoría textual: `StatementKind::Direct`, `scopedStatementStarts` y
  `controlStatementStarts` tienen cero apariciones; CMake, declaración y registro default contienen
  exactamente una entrada del módulo; manifest/design/allocation contienen exactamente una fila.
- No se ejecutaron Host.Parser completo, CTest, TL-LANG, regresiones, minisuite, baseline, deploy ni
  instalación.
