# Tanda 17 — Congelamiento y promoción / Freeze and promotion

## Resultado / Outcome

ES: Se implementó el contrato final de freeze: revisiones independientes de sintaxis, semántica,
ABI Win64 y NSMETA; tiers de conformidad; paridad bilingüe; política RC; y evidencia exhaustiva de
promoción. EN: The final freeze contract covers independent syntax, semantics, Win64 ABI, and NSMETA
revisions, conformance tiers, bilingual parity, RC policy, and exhaustive promotion evidence.

## Archivos / Files

- Nuevos `ReleaseSemantics.h/.cpp`: gate fail-closed de freeze y promoción.
- Nueva `NativeSharp3T17ReleaseTests.cpp`: siete comprobaciones dirigidas.
- `CMakeLists.txt`: integra el contrato y target T17.

## Mejora / Improvement

NativeSharp 3.0 no puede declararse estable por accidente: un baseline ausente, P0/P1, falta de una
configuración de conformidad, docs disparejas, migración no validada, artefactos no reproducibles o
hashes ausentes bloquean promoción y enumeran evidencia faltante. Promotion is evidence-driven and
the normative target is explicitly Windows x64.

## Estado de promoción / Promotion state

La implementación de la tanda queda terminada, pero **no se crea tag ni se declara promoción
oficial** en este commit. Alberto ejecutará el baseline externo final; hasta recibir y registrar esa
evidencia, `promotionStatus` debe permanecer `NotReady`. No se realizaron operaciones remotas.

The batch implementation is complete, but no tag or official promotion is claimed. The external
baseline remains under Alberto's control and promotion stays `NotReady` until all evidence exists.

## Validación / Validation

`nsc` O0 y `nsc_ns3_t17_release_tests`; sin baseline ni CTest general.
