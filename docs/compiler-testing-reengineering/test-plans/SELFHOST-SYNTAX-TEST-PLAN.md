# Plan de pruebas — SelfHost.Syntax

## 1. Alcance

Plan desde cero para 5 contratos y 36 casos diseñados. No reutiliza el nombre histórico como oracle y no implementa ni ejecuta pruebas.

## 2. Estrategia futura

- Casos internos en un ejecutable unitario consolidado por componente, no un ejecutable por caso.
- Casos de lenguaje/proceso mediante el compilador construido y fixtures `.ns` externos mínimos.
- Oracles estructurales o semánticos; los mensajes sólo se comparan por código/categoría/campos estables.
- Self-host, escala y publicación costosa quedan `NIGHTLY`; los contratos pequeños y de frontera quedan `PR`.

## 3. Contratos

| Contrato | Nombre | Capa | Oracle |
|---|---|---|---|
| CSH-SYN-001 | Syntax node construction and scalar fields | unit | all field values before/after setter |
| CSH-SYN-002 | Syntax node relationships | unit | slots children order and count |
| CSH-SYN-003 | Syntax node and tree freeze | unit | state transition and structural hash |
| CSH-SYN-004 | Syntax tree node arena and root | unit | IDs count root and node equality |
| CSH-SYN-005 | Deterministic syntax dump | unit | exact dump and depth stress outcome |

## 4. Casos diseñados

| Caso | Contrato | Objetivo | Entrada | Esperado | Capa | Frecuencia |
|---|---|---|---|---|---|---|
| TSH-00945 | CSH-SYN-001 | minimal valid contract: Syntax node construction and scalar fields | minimum valid input named by contract | exact successful output and invariant; oracle: all field values before/after setter | unit | PR |
| TSH-00946 | CSH-SYN-001 | representative valid contract: Syntax node construction and scalar fields | all ordinary fields and relationships | complete output and side effects match oracle; oracle: all field values before/after setter | unit | PR |
| TSH-00947 | CSH-SYN-001 | missing producer prerequisite: Syntax node construction and scalar fields | otherwise valid input with one prerequisite absent | reject at owner boundary with no downstream publication; oracle: all field values before/after setter | unit | PR |
| TSH-00948 | CSH-SYN-001 | malformed relationship or value: Syntax node construction and scalar fields | invalid ordering type layout token metadata or ownership relation | owned error and no accepted partial output; oracle: all field values before/after setter | unit | PR |
| TSH-00949 | CSH-SYN-001 | empty singleton duplicate and extrema: Syntax node construction and scalar fields | empty one duplicate min max null and non-ASCII where applicable | documented boundary behavior for every applicable row; oracle: all field values before/after setter | unit | PR |
| TSH-00950 | CSH-SYN-001 | all named modes and variants: Syntax node construction and scalar fields | every enum mode kind operator and ABI variant owned by contract | each variant preserves its named contract; oracle: all field values before/after setter | unit | PR |
| TSH-00951 | CSH-SYN-002 | minimal valid contract: Syntax node relationships | minimum valid input named by contract | exact successful output and invariant; oracle: slots children order and count | unit | PR |
| TSH-00952 | CSH-SYN-002 | representative valid contract: Syntax node relationships | all ordinary fields and relationships | complete output and side effects match oracle; oracle: slots children order and count | unit | PR |
| TSH-00953 | CSH-SYN-002 | missing producer prerequisite: Syntax node relationships | otherwise valid input with one prerequisite absent | reject at owner boundary with no downstream publication; oracle: slots children order and count | unit | PR |
| TSH-00954 | CSH-SYN-002 | malformed relationship or value: Syntax node relationships | invalid ordering type layout token metadata or ownership relation | owned error and no accepted partial output; oracle: slots children order and count | unit | PR |
| TSH-00955 | CSH-SYN-002 | empty singleton duplicate and extrema: Syntax node relationships | empty one duplicate min max null and non-ASCII where applicable | documented boundary behavior for every applicable row; oracle: slots children order and count | unit | PR |
| TSH-00956 | CSH-SYN-002 | all named modes and variants: Syntax node relationships | every enum mode kind operator and ABI variant owned by contract | each variant preserves its named contract; oracle: slots children order and count | unit | PR |
| TSH-00957 | CSH-SYN-002 | repeatability and input-order perturbation: Syntax node relationships | same semantic input under repeated/permuted construction | identical semantic structure or bytes in canonical order; oracle: slots children order and count | unit | PR |
| TSH-00958 | CSH-SYN-002 | depth breadth and scale boundary: Syntax node relationships | large graph tree source or declaration set at documented limits | bounded completion or explicitly documented limitation without silent truncation; oracle: slots children order and count | unit | NIGHTLY |
| TSH-00959 | CSH-SYN-003 | minimal valid contract: Syntax node and tree freeze | minimum valid input named by contract | exact successful output and invariant; oracle: state transition and structural hash | unit | PR |
| TSH-00960 | CSH-SYN-003 | representative valid contract: Syntax node and tree freeze | all ordinary fields and relationships | complete output and side effects match oracle; oracle: state transition and structural hash | unit | PR |
| TSH-00961 | CSH-SYN-003 | missing producer prerequisite: Syntax node and tree freeze | otherwise valid input with one prerequisite absent | reject at owner boundary with no downstream publication; oracle: state transition and structural hash | unit | PR |
| TSH-00962 | CSH-SYN-003 | malformed relationship or value: Syntax node and tree freeze | invalid ordering type layout token metadata or ownership relation | owned error and no accepted partial output; oracle: state transition and structural hash | unit | PR |
| TSH-00963 | CSH-SYN-003 | empty singleton duplicate and extrema: Syntax node and tree freeze | empty one duplicate min max null and non-ASCII where applicable | documented boundary behavior for every applicable row; oracle: state transition and structural hash | unit | PR |
| TSH-00964 | CSH-SYN-003 | all named modes and variants: Syntax node and tree freeze | every enum mode kind operator and ABI variant owned by contract | each variant preserves its named contract; oracle: state transition and structural hash | unit | PR |
| TSH-00965 | CSH-SYN-003 | depth breadth and scale boundary: Syntax node and tree freeze | large graph tree source or declaration set at documented limits | bounded completion or explicitly documented limitation without silent truncation; oracle: state transition and structural hash | unit | NIGHTLY |
| TSH-00966 | CSH-SYN-004 | minimal valid contract: Syntax tree node arena and root | minimum valid input named by contract | exact successful output and invariant; oracle: IDs count root and node equality | unit | PR |
| TSH-00967 | CSH-SYN-004 | representative valid contract: Syntax tree node arena and root | all ordinary fields and relationships | complete output and side effects match oracle; oracle: IDs count root and node equality | unit | PR |
| TSH-00968 | CSH-SYN-004 | missing producer prerequisite: Syntax tree node arena and root | otherwise valid input with one prerequisite absent | reject at owner boundary with no downstream publication; oracle: IDs count root and node equality | unit | PR |
| TSH-00969 | CSH-SYN-004 | malformed relationship or value: Syntax tree node arena and root | invalid ordering type layout token metadata or ownership relation | owned error and no accepted partial output; oracle: IDs count root and node equality | unit | PR |
| TSH-00970 | CSH-SYN-004 | empty singleton duplicate and extrema: Syntax tree node arena and root | empty one duplicate min max null and non-ASCII where applicable | documented boundary behavior for every applicable row; oracle: IDs count root and node equality | unit | PR |
| TSH-00971 | CSH-SYN-004 | all named modes and variants: Syntax tree node arena and root | every enum mode kind operator and ABI variant owned by contract | each variant preserves its named contract; oracle: IDs count root and node equality | unit | PR |
| TSH-00972 | CSH-SYN-004 | repeatability and input-order perturbation: Syntax tree node arena and root | same semantic input under repeated/permuted construction | identical semantic structure or bytes in canonical order; oracle: IDs count root and node equality | unit | PR |
| TSH-00973 | CSH-SYN-004 | depth breadth and scale boundary: Syntax tree node arena and root | large graph tree source or declaration set at documented limits | bounded completion or explicitly documented limitation without silent truncation; oracle: IDs count root and node equality | unit | NIGHTLY |
| TSH-00974 | CSH-SYN-005 | minimal valid contract: Deterministic syntax dump | minimum valid input named by contract | exact successful output and invariant; oracle: exact dump and depth stress outcome | unit | PR |
| TSH-00975 | CSH-SYN-005 | representative valid contract: Deterministic syntax dump | all ordinary fields and relationships | complete output and side effects match oracle; oracle: exact dump and depth stress outcome | unit | PR |
| TSH-00976 | CSH-SYN-005 | missing producer prerequisite: Deterministic syntax dump | otherwise valid input with one prerequisite absent | reject at owner boundary with no downstream publication; oracle: exact dump and depth stress outcome | unit | PR |
| TSH-00977 | CSH-SYN-005 | malformed relationship or value: Deterministic syntax dump | invalid ordering type layout token metadata or ownership relation | owned error and no accepted partial output; oracle: exact dump and depth stress outcome | unit | PR |
| TSH-00978 | CSH-SYN-005 | empty singleton duplicate and extrema: Deterministic syntax dump | empty one duplicate min max null and non-ASCII where applicable | documented boundary behavior for every applicable row; oracle: exact dump and depth stress outcome | unit | PR |
| TSH-00979 | CSH-SYN-005 | all named modes and variants: Deterministic syntax dump | every enum mode kind operator and ABI variant owned by contract | each variant preserves its named contract; oracle: exact dump and depth stress outcome | unit | PR |
| TSH-00980 | CSH-SYN-005 | repeatability and input-order perturbation: Deterministic syntax dump | same semantic input under repeated/permuted construction | identical semantic structure or bytes in canonical order; oracle: exact dump and depth stress outcome | unit | PR |

## 5. Gate de implementación futura

Cada caso deberá ejecutarse de forma individual, conservar evidencia y mapearse al contrato. El componente no podrá declararse cubierto por el simple éxito de un runner agregado. Coverage instrumental pertenece a la Fase 3.
