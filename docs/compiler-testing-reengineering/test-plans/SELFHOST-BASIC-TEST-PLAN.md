# Plan de pruebas — SelfHost.Basic

## 1. Alcance

Plan desde cero para 4 contratos y 28 casos diseñados. No reutiliza el nombre histórico como oracle y no implementa ni ejecuta pruebas.

## 2. Estrategia futura

- Casos internos en un ejecutable unitario consolidado por componente, no un ejecutable por caso.
- Casos de lenguaje/proceso mediante el compilador construido y fixtures `.ns` externos mínimos.
- Oracles estructurales o semánticos; los mensajes sólo se comparan por código/categoría/campos estables.
- Self-host, escala y publicación costosa quedan `NIGHTLY`; los contratos pequeños y de frontera quedan `PR`.

## 3. Contratos

| Contrato | Nombre | Capa | Oracle |
|---|---|---|---|
| CSH-BAS-001 | Typed source identities and compiler results | unit | exact fields validity equality length and exit code |
| CSH-BAS-002 | Generational arena handles | unit | address generation size usage and stale-handle truth table |
| CSH-BAS-003 | Stable text hash | unit | exact known vectors and repeated-input identity |
| CSH-BAS-004 | Basic ownership and value lifetime | unit | generation transition and lifetime boundary |

## 4. Casos diseñados

| Caso | Contrato | Objetivo | Entrada | Esperado | Capa | Frecuencia |
|---|---|---|---|---|---|---|
| TSH-00001 | CSH-BAS-001 | minimal valid contract: Typed source identities and compiler results | minimum valid input named by contract | exact successful output and invariant; oracle: exact fields validity equality length and exit code | unit | PR |
| TSH-00002 | CSH-BAS-001 | representative valid contract: Typed source identities and compiler results | all ordinary fields and relationships | complete output and side effects match oracle; oracle: exact fields validity equality length and exit code | unit | PR |
| TSH-00003 | CSH-BAS-001 | missing producer prerequisite: Typed source identities and compiler results | otherwise valid input with one prerequisite absent | reject at owner boundary with no downstream publication; oracle: exact fields validity equality length and exit code | unit | PR |
| TSH-00004 | CSH-BAS-001 | malformed relationship or value: Typed source identities and compiler results | invalid ordering type layout token metadata or ownership relation | owned error and no accepted partial output; oracle: exact fields validity equality length and exit code | unit | PR |
| TSH-00005 | CSH-BAS-001 | empty singleton duplicate and extrema: Typed source identities and compiler results | empty one duplicate min max null and non-ASCII where applicable | documented boundary behavior for every applicable row; oracle: exact fields validity equality length and exit code | unit | PR |
| TSH-00006 | CSH-BAS-001 | all named modes and variants: Typed source identities and compiler results | every enum mode kind operator and ABI variant owned by contract | each variant preserves its named contract; oracle: exact fields validity equality length and exit code | unit | PR |
| TSH-00007 | CSH-BAS-001 | repeatability and input-order perturbation: Typed source identities and compiler results | same semantic input under repeated/permuted construction | identical semantic structure or bytes in canonical order; oracle: exact fields validity equality length and exit code | unit | PR |
| TSH-00008 | CSH-BAS-001 | depth breadth and scale boundary: Typed source identities and compiler results | large graph tree source or declaration set at documented limits | bounded completion or explicitly documented limitation without silent truncation; oracle: exact fields validity equality length and exit code | unit | NIGHTLY |
| TSH-00009 | CSH-BAS-002 | minimal valid contract: Generational arena handles | minimum valid input named by contract | exact successful output and invariant; oracle: address generation size usage and stale-handle truth table | unit | PR |
| TSH-00010 | CSH-BAS-002 | representative valid contract: Generational arena handles | all ordinary fields and relationships | complete output and side effects match oracle; oracle: address generation size usage and stale-handle truth table | unit | PR |
| TSH-00011 | CSH-BAS-002 | missing producer prerequisite: Generational arena handles | otherwise valid input with one prerequisite absent | reject at owner boundary with no downstream publication; oracle: address generation size usage and stale-handle truth table | unit | PR |
| TSH-00012 | CSH-BAS-002 | malformed relationship or value: Generational arena handles | invalid ordering type layout token metadata or ownership relation | owned error and no accepted partial output; oracle: address generation size usage and stale-handle truth table | unit | PR |
| TSH-00013 | CSH-BAS-002 | empty singleton duplicate and extrema: Generational arena handles | empty one duplicate min max null and non-ASCII where applicable | documented boundary behavior for every applicable row; oracle: address generation size usage and stale-handle truth table | unit | PR |
| TSH-00014 | CSH-BAS-002 | all named modes and variants: Generational arena handles | every enum mode kind operator and ABI variant owned by contract | each variant preserves its named contract; oracle: address generation size usage and stale-handle truth table | unit | PR |
| TSH-00015 | CSH-BAS-002 | depth breadth and scale boundary: Generational arena handles | large graph tree source or declaration set at documented limits | bounded completion or explicitly documented limitation without silent truncation; oracle: address generation size usage and stale-handle truth table | unit | NIGHTLY |
| TSH-00016 | CSH-BAS-003 | minimal valid contract: Stable text hash | minimum valid input named by contract | exact successful output and invariant; oracle: exact known vectors and repeated-input identity | unit | PR |
| TSH-00017 | CSH-BAS-003 | representative valid contract: Stable text hash | all ordinary fields and relationships | complete output and side effects match oracle; oracle: exact known vectors and repeated-input identity | unit | PR |
| TSH-00018 | CSH-BAS-003 | missing producer prerequisite: Stable text hash | otherwise valid input with one prerequisite absent | reject at owner boundary with no downstream publication; oracle: exact known vectors and repeated-input identity | unit | PR |
| TSH-00019 | CSH-BAS-003 | malformed relationship or value: Stable text hash | invalid ordering type layout token metadata or ownership relation | owned error and no accepted partial output; oracle: exact known vectors and repeated-input identity | unit | PR |
| TSH-00020 | CSH-BAS-003 | empty singleton duplicate and extrema: Stable text hash | empty one duplicate min max null and non-ASCII where applicable | documented boundary behavior for every applicable row; oracle: exact known vectors and repeated-input identity | unit | PR |
| TSH-00021 | CSH-BAS-003 | all named modes and variants: Stable text hash | every enum mode kind operator and ABI variant owned by contract | each variant preserves its named contract; oracle: exact known vectors and repeated-input identity | unit | PR |
| TSH-00022 | CSH-BAS-003 | repeatability and input-order perturbation: Stable text hash | same semantic input under repeated/permuted construction | identical semantic structure or bytes in canonical order; oracle: exact known vectors and repeated-input identity | unit | PR |
| TSH-00023 | CSH-BAS-004 | minimal valid contract: Basic ownership and value lifetime | minimum valid input named by contract | exact successful output and invariant; oracle: generation transition and lifetime boundary | unit | PR |
| TSH-00024 | CSH-BAS-004 | representative valid contract: Basic ownership and value lifetime | all ordinary fields and relationships | complete output and side effects match oracle; oracle: generation transition and lifetime boundary | unit | PR |
| TSH-00025 | CSH-BAS-004 | missing producer prerequisite: Basic ownership and value lifetime | otherwise valid input with one prerequisite absent | reject at owner boundary with no downstream publication; oracle: generation transition and lifetime boundary | unit | PR |
| TSH-00026 | CSH-BAS-004 | malformed relationship or value: Basic ownership and value lifetime | invalid ordering type layout token metadata or ownership relation | owned error and no accepted partial output; oracle: generation transition and lifetime boundary | unit | PR |
| TSH-00027 | CSH-BAS-004 | empty singleton duplicate and extrema: Basic ownership and value lifetime | empty one duplicate min max null and non-ASCII where applicable | documented boundary behavior for every applicable row; oracle: generation transition and lifetime boundary | unit | PR |
| TSH-00028 | CSH-BAS-004 | all named modes and variants: Basic ownership and value lifetime | every enum mode kind operator and ABI variant owned by contract | each variant preserves its named contract; oracle: generation transition and lifetime boundary | unit | PR |

## 5. Gate de implementación futura

Cada caso deberá ejecutarse de forma individual, conservar evidencia y mapearse al contrato. El componente no podrá declararse cubierto por el simple éxito de un runner agregado. Coverage instrumental pertenece a la Fase 3.
