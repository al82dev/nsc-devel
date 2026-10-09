# Plan de pruebas — SelfHost.Symbols

## 1. Alcance

Plan desde cero para 5 contratos y 35 casos diseñados. No reutiliza el nombre histórico como oracle y no implementa ni ejecuta pruebas.

## 2. Estrategia futura

- Casos internos en un ejecutable unitario consolidado por componente, no un ejecutable por caso.
- Casos de lenguaje/proceso mediante el compilador construido y fixtures `.ns` externos mínimos.
- Oracles estructurales o semánticos; los mensajes sólo se comparan por código/categoría/campos estables.
- Self-host, escala y publicación costosa quedan `NIGHTLY`; los contratos pequeños y de frontera quedan `PR`.

## 3. Contratos

| Contrato | Nombre | Capa | Oracle |
|---|---|---|---|
| CSH-SYM-001 | Symbol kind vocabulary | unit | exhaustive enum/name table |
| CSH-SYM-002 | Symbol identity and declaration metadata | unit | exact fields before/after mutation |
| CSH-SYM-003 | Symbol member and layout metadata | unit | member table and exact layout values |
| CSH-SYM-004 | Symbol constants and reachability | unit | flag/value transition table |
| CSH-SYM-005 | Lexical scope declaration and lookup | unit | count/order/lookup and parent identity |

## 4. Casos diseñados

| Caso | Contrato | Objetivo | Entrada | Esperado | Capa | Frecuencia |
|---|---|---|---|---|---|---|
| TSH-00910 | CSH-SYM-001 | minimal valid contract: Symbol kind vocabulary | minimum valid input named by contract | exact successful output and invariant; oracle: exhaustive enum/name table | unit | PR |
| TSH-00911 | CSH-SYM-001 | representative valid contract: Symbol kind vocabulary | all ordinary fields and relationships | complete output and side effects match oracle; oracle: exhaustive enum/name table | unit | PR |
| TSH-00912 | CSH-SYM-001 | missing producer prerequisite: Symbol kind vocabulary | otherwise valid input with one prerequisite absent | reject at owner boundary with no downstream publication; oracle: exhaustive enum/name table | unit | PR |
| TSH-00913 | CSH-SYM-001 | malformed relationship or value: Symbol kind vocabulary | invalid ordering type layout token metadata or ownership relation | owned error and no accepted partial output; oracle: exhaustive enum/name table | unit | PR |
| TSH-00914 | CSH-SYM-001 | empty singleton duplicate and extrema: Symbol kind vocabulary | empty one duplicate min max null and non-ASCII where applicable | documented boundary behavior for every applicable row; oracle: exhaustive enum/name table | unit | PR |
| TSH-00915 | CSH-SYM-001 | all named modes and variants: Symbol kind vocabulary | every enum mode kind operator and ABI variant owned by contract | each variant preserves its named contract; oracle: exhaustive enum/name table | unit | PR |
| TSH-00916 | CSH-SYM-001 | repeatability and input-order perturbation: Symbol kind vocabulary | same semantic input under repeated/permuted construction | identical semantic structure or bytes in canonical order; oracle: exhaustive enum/name table | unit | PR |
| TSH-00917 | CSH-SYM-002 | minimal valid contract: Symbol identity and declaration metadata | minimum valid input named by contract | exact successful output and invariant; oracle: exact fields before/after mutation | unit | PR |
| TSH-00918 | CSH-SYM-002 | representative valid contract: Symbol identity and declaration metadata | all ordinary fields and relationships | complete output and side effects match oracle; oracle: exact fields before/after mutation | unit | PR |
| TSH-00919 | CSH-SYM-002 | missing producer prerequisite: Symbol identity and declaration metadata | otherwise valid input with one prerequisite absent | reject at owner boundary with no downstream publication; oracle: exact fields before/after mutation | unit | PR |
| TSH-00920 | CSH-SYM-002 | malformed relationship or value: Symbol identity and declaration metadata | invalid ordering type layout token metadata or ownership relation | owned error and no accepted partial output; oracle: exact fields before/after mutation | unit | PR |
| TSH-00921 | CSH-SYM-002 | empty singleton duplicate and extrema: Symbol identity and declaration metadata | empty one duplicate min max null and non-ASCII where applicable | documented boundary behavior for every applicable row; oracle: exact fields before/after mutation | unit | PR |
| TSH-00922 | CSH-SYM-002 | all named modes and variants: Symbol identity and declaration metadata | every enum mode kind operator and ABI variant owned by contract | each variant preserves its named contract; oracle: exact fields before/after mutation | unit | PR |
| TSH-00923 | CSH-SYM-002 | repeatability and input-order perturbation: Symbol identity and declaration metadata | same semantic input under repeated/permuted construction | identical semantic structure or bytes in canonical order; oracle: exact fields before/after mutation | unit | PR |
| TSH-00924 | CSH-SYM-003 | minimal valid contract: Symbol member and layout metadata | minimum valid input named by contract | exact successful output and invariant; oracle: member table and exact layout values | unit | PR |
| TSH-00925 | CSH-SYM-003 | representative valid contract: Symbol member and layout metadata | all ordinary fields and relationships | complete output and side effects match oracle; oracle: member table and exact layout values | unit | PR |
| TSH-00926 | CSH-SYM-003 | missing producer prerequisite: Symbol member and layout metadata | otherwise valid input with one prerequisite absent | reject at owner boundary with no downstream publication; oracle: member table and exact layout values | unit | PR |
| TSH-00927 | CSH-SYM-003 | malformed relationship or value: Symbol member and layout metadata | invalid ordering type layout token metadata or ownership relation | owned error and no accepted partial output; oracle: member table and exact layout values | unit | PR |
| TSH-00928 | CSH-SYM-003 | empty singleton duplicate and extrema: Symbol member and layout metadata | empty one duplicate min max null and non-ASCII where applicable | documented boundary behavior for every applicable row; oracle: member table and exact layout values | unit | PR |
| TSH-00929 | CSH-SYM-003 | all named modes and variants: Symbol member and layout metadata | every enum mode kind operator and ABI variant owned by contract | each variant preserves its named contract; oracle: member table and exact layout values | unit | PR |
| TSH-00930 | CSH-SYM-003 | repeatability and input-order perturbation: Symbol member and layout metadata | same semantic input under repeated/permuted construction | identical semantic structure or bytes in canonical order; oracle: member table and exact layout values | unit | PR |
| TSH-00931 | CSH-SYM-004 | minimal valid contract: Symbol constants and reachability | minimum valid input named by contract | exact successful output and invariant; oracle: flag/value transition table | unit | PR |
| TSH-00932 | CSH-SYM-004 | representative valid contract: Symbol constants and reachability | all ordinary fields and relationships | complete output and side effects match oracle; oracle: flag/value transition table | unit | PR |
| TSH-00933 | CSH-SYM-004 | missing producer prerequisite: Symbol constants and reachability | otherwise valid input with one prerequisite absent | reject at owner boundary with no downstream publication; oracle: flag/value transition table | unit | PR |
| TSH-00934 | CSH-SYM-004 | malformed relationship or value: Symbol constants and reachability | invalid ordering type layout token metadata or ownership relation | owned error and no accepted partial output; oracle: flag/value transition table | unit | PR |
| TSH-00935 | CSH-SYM-004 | empty singleton duplicate and extrema: Symbol constants and reachability | empty one duplicate min max null and non-ASCII where applicable | documented boundary behavior for every applicable row; oracle: flag/value transition table | unit | PR |
| TSH-00936 | CSH-SYM-004 | all named modes and variants: Symbol constants and reachability | every enum mode kind operator and ABI variant owned by contract | each variant preserves its named contract; oracle: flag/value transition table | unit | PR |
| TSH-00937 | CSH-SYM-004 | depth breadth and scale boundary: Symbol constants and reachability | large graph tree source or declaration set at documented limits | bounded completion or explicitly documented limitation without silent truncation; oracle: flag/value transition table | unit | NIGHTLY |
| TSH-00938 | CSH-SYM-005 | minimal valid contract: Lexical scope declaration and lookup | minimum valid input named by contract | exact successful output and invariant; oracle: count/order/lookup and parent identity | unit | PR |
| TSH-00939 | CSH-SYM-005 | representative valid contract: Lexical scope declaration and lookup | all ordinary fields and relationships | complete output and side effects match oracle; oracle: count/order/lookup and parent identity | unit | PR |
| TSH-00940 | CSH-SYM-005 | missing producer prerequisite: Lexical scope declaration and lookup | otherwise valid input with one prerequisite absent | reject at owner boundary with no downstream publication; oracle: count/order/lookup and parent identity | unit | PR |
| TSH-00941 | CSH-SYM-005 | malformed relationship or value: Lexical scope declaration and lookup | invalid ordering type layout token metadata or ownership relation | owned error and no accepted partial output; oracle: count/order/lookup and parent identity | unit | PR |
| TSH-00942 | CSH-SYM-005 | empty singleton duplicate and extrema: Lexical scope declaration and lookup | empty one duplicate min max null and non-ASCII where applicable | documented boundary behavior for every applicable row; oracle: count/order/lookup and parent identity | unit | PR |
| TSH-00943 | CSH-SYM-005 | all named modes and variants: Lexical scope declaration and lookup | every enum mode kind operator and ABI variant owned by contract | each variant preserves its named contract; oracle: count/order/lookup and parent identity | unit | PR |
| TSH-00944 | CSH-SYM-005 | repeatability and input-order perturbation: Lexical scope declaration and lookup | same semantic input under repeated/permuted construction | identical semantic structure or bytes in canonical order; oracle: count/order/lookup and parent identity | unit | PR |

## 5. Gate de implementación futura

Cada caso deberá ejecutarse de forma individual, conservar evidencia y mapearse al contrato. El componente no podrá declararse cubierto por el simple éxito de un runner agregado. Coverage instrumental pertenece a la Fase 3.
