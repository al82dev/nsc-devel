# NS3-T15-F01 — Matriz combinatoria / Feature interaction matrix

## ES

La matriz canónica genera casos reproducibles con identidad y seed únicos. Cubre los pares críticos variants/ownership, generics/traits, ownership/coroutines, errors/coroutines, modules/FFI y metaprogramming/modules, además de tríos de alto riesgo que cambian MIR u ownership.

Cada caso declara riesgo, impacto y soporte de minimización. Un fallo se reduce conservando el seed y únicamente áreas presentes en el caso original; no se permite inflar cobertura con identidades duplicadas ni introducir features durante reducción. Los huecos aceptados se agregan como casos nuevos, nunca relajando pares obligatorios.

Prueba dirigida: `nsc_ns3_t15_interaction_matrix_tests` valida generación determinista, pares/tríos, seeds, prioridad y minimización causal.

## EN

The canonical matrix generates reproducible cases with unique identities and seeds. It covers the critical variants/ownership, generics/traits, ownership/coroutines, errors/coroutines, modules/FFI, and metaprogramming/modules pairs, plus high-risk triples that change MIR or ownership.

Each case declares risk, impact, and minimization support. A failure is reduced while preserving its seed and retaining only areas present in the original case; duplicate identities cannot inflate coverage and reduction cannot introduce features. Accepted gaps become new cases rather than weakening mandatory pairs.

Focused test: `nsc_ns3_t15_interaction_matrix_tests` validates deterministic generation, pairs/triples, seeds, prioritization, and causal minimization.
