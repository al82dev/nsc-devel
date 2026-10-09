# Plan de pruebas — SelfHost.Project

## 1. Alcance

Plan desde cero para 3 contratos y 22 casos diseñados. No reutiliza el nombre histórico como oracle y no implementa ni ejecuta pruebas.

## 2. Estrategia futura

- Casos internos en un ejecutable unitario consolidado por componente, no un ejecutable por caso.
- Casos de lenguaje/proceso mediante el compilador construido y fixtures `.ns` externos mínimos.
- Oracles estructurales o semánticos; los mensajes sólo se comparan por código/categoría/campos estables.
- Self-host, escala y publicación costosa quedan `NIGHTLY`; los contratos pequeños y de frontera quedan `PR`.

## 3. Contratos

| Contrato | Nombre | Capa | Oracle |
|---|---|---|---|
| CSH-PRO-001 | Self-host project identity and source globs | build-contract | resolved source set compared with canonical 27 sources |
| CSH-PRO-002 | Self-host project dependency closure | build-contract | exact normalized reference set and order |
| CSH-PRO-003 | Project/source inventory coherence | build-contract | resolved paths cardinality uniqueness and component ownership |

## 4. Casos diseñados

| Caso | Contrato | Objetivo | Entrada | Esperado | Capa | Frecuencia |
|---|---|---|---|---|---|---|
| TSH-00579 | CSH-PRO-001 | minimal valid contract: Self-host project identity and source globs | minimum valid input named by contract | exact successful output and invariant; oracle: resolved source set compared with canonical 27 sources | build-contract | PR |
| TSH-00580 | CSH-PRO-001 | representative valid contract: Self-host project identity and source globs | all ordinary fields and relationships | complete output and side effects match oracle; oracle: resolved source set compared with canonical 27 sources | build-contract | PR |
| TSH-00581 | CSH-PRO-001 | missing producer prerequisite: Self-host project identity and source globs | otherwise valid input with one prerequisite absent | reject at owner boundary with no downstream publication; oracle: resolved source set compared with canonical 27 sources | build-contract | PR |
| TSH-00582 | CSH-PRO-001 | malformed relationship or value: Self-host project identity and source globs | invalid ordering type layout token metadata or ownership relation | owned error and no accepted partial output; oracle: resolved source set compared with canonical 27 sources | build-contract | PR |
| TSH-00583 | CSH-PRO-001 | empty singleton duplicate and extrema: Self-host project identity and source globs | empty one duplicate min max null and non-ASCII where applicable | documented boundary behavior for every applicable row; oracle: resolved source set compared with canonical 27 sources | build-contract | PR |
| TSH-00584 | CSH-PRO-001 | all named modes and variants: Self-host project identity and source globs | every enum mode kind operator and ABI variant owned by contract | each variant preserves its named contract; oracle: resolved source set compared with canonical 27 sources | build-contract | PR |
| TSH-00585 | CSH-PRO-001 | repeatability and input-order perturbation: Self-host project identity and source globs | same semantic input under repeated/permuted construction | identical semantic structure or bytes in canonical order; oracle: resolved source set compared with canonical 27 sources | build-contract | PR |
| TSH-00586 | CSH-PRO-001 | depth breadth and scale boundary: Self-host project identity and source globs | large graph tree source or declaration set at documented limits | bounded completion or explicitly documented limitation without silent truncation; oracle: resolved source set compared with canonical 27 sources | build-contract | NIGHTLY |
| TSH-00587 | CSH-PRO-002 | minimal valid contract: Self-host project dependency closure | minimum valid input named by contract | exact successful output and invariant; oracle: exact normalized reference set and order | build-contract | PR |
| TSH-00588 | CSH-PRO-002 | representative valid contract: Self-host project dependency closure | all ordinary fields and relationships | complete output and side effects match oracle; oracle: exact normalized reference set and order | build-contract | PR |
| TSH-00589 | CSH-PRO-002 | missing producer prerequisite: Self-host project dependency closure | otherwise valid input with one prerequisite absent | reject at owner boundary with no downstream publication; oracle: exact normalized reference set and order | build-contract | PR |
| TSH-00590 | CSH-PRO-002 | malformed relationship or value: Self-host project dependency closure | invalid ordering type layout token metadata or ownership relation | owned error and no accepted partial output; oracle: exact normalized reference set and order | build-contract | PR |
| TSH-00591 | CSH-PRO-002 | empty singleton duplicate and extrema: Self-host project dependency closure | empty one duplicate min max null and non-ASCII where applicable | documented boundary behavior for every applicable row; oracle: exact normalized reference set and order | build-contract | PR |
| TSH-00592 | CSH-PRO-002 | all named modes and variants: Self-host project dependency closure | every enum mode kind operator and ABI variant owned by contract | each variant preserves its named contract; oracle: exact normalized reference set and order | build-contract | PR |
| TSH-00593 | CSH-PRO-002 | depth breadth and scale boundary: Self-host project dependency closure | large graph tree source or declaration set at documented limits | bounded completion or explicitly documented limitation without silent truncation; oracle: exact normalized reference set and order | build-contract | NIGHTLY |
| TSH-00594 | CSH-PRO-003 | minimal valid contract: Project/source inventory coherence | minimum valid input named by contract | exact successful output and invariant; oracle: resolved paths cardinality uniqueness and component ownership | build-contract | PR |
| TSH-00595 | CSH-PRO-003 | representative valid contract: Project/source inventory coherence | all ordinary fields and relationships | complete output and side effects match oracle; oracle: resolved paths cardinality uniqueness and component ownership | build-contract | PR |
| TSH-00596 | CSH-PRO-003 | missing producer prerequisite: Project/source inventory coherence | otherwise valid input with one prerequisite absent | reject at owner boundary with no downstream publication; oracle: resolved paths cardinality uniqueness and component ownership | build-contract | PR |
| TSH-00597 | CSH-PRO-003 | malformed relationship or value: Project/source inventory coherence | invalid ordering type layout token metadata or ownership relation | owned error and no accepted partial output; oracle: resolved paths cardinality uniqueness and component ownership | build-contract | PR |
| TSH-00598 | CSH-PRO-003 | empty singleton duplicate and extrema: Project/source inventory coherence | empty one duplicate min max null and non-ASCII where applicable | documented boundary behavior for every applicable row; oracle: resolved paths cardinality uniqueness and component ownership | build-contract | PR |
| TSH-00599 | CSH-PRO-003 | all named modes and variants: Project/source inventory coherence | every enum mode kind operator and ABI variant owned by contract | each variant preserves its named contract; oracle: resolved paths cardinality uniqueness and component ownership | build-contract | PR |
| TSH-00600 | CSH-PRO-003 | depth breadth and scale boundary: Project/source inventory coherence | large graph tree source or declaration set at documented limits | bounded completion or explicitly documented limitation without silent truncation; oracle: resolved paths cardinality uniqueness and component ownership | build-contract | NIGHTLY |

## 5. Gate de implementación futura

Cada caso deberá ejecutarse de forma individual, conservar evidencia y mapearse al contrato. El componente no podrá declararse cubierto por el simple éxito de un runner agregado. Coverage instrumental pertenece a la Fase 3.
