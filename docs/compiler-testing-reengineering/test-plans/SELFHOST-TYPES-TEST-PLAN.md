# Plan de pruebas — SelfHost.Types

## 1. Alcance

Plan desde cero para 7 contratos y 47 casos diseñados. No reutiliza el nombre histórico como oracle y no implementa ni ejecuta pruebas.

## 2. Estrategia futura

- Casos internos en un ejecutable unitario consolidado por componente, no un ejecutable por caso.
- Casos de lenguaje/proceso mediante el compilador construido y fixtures `.ns` externos mínimos.
- Oracles estructurales o semánticos; los mensajes sólo se comparan por código/categoría/campos estables.
- Self-host, escala y publicación costosa quedan `NIGHTLY`; los contratos pequeños y de frontera quedan `PR`.

## 3. Contratos

| Contrato | Nombre | Capa | Oracle |
|---|---|---|---|
| CSH-TYP-001 | Semantic and ownership type vocabularies | unit | exhaustive enum behavior in consumers |
| CSH-TYP-002 | Semantic type value and layout | unit | all fields and transition table |
| CSH-TYP-003 | Function parameter type list | unit | exact ordered IDs and count |
| CSH-TYP-004 | Builtin type table initialization | unit | complete builtin name/kind/layout inventory |
| CSH-TYP-005 | Type lookup and interning | component | ID equality descriptors count and mutation stability |
| CSH-TYP-006 | String literal type construction | unit | kind display element count and ID reuse |
| CSH-TYP-007 | Function type construction and numeric predicates | unit | signature descriptor and predicate truth table |

## 4. Casos diseñados

| Caso | Contrato | Objetivo | Entrada | Esperado | Capa | Frecuencia |
|---|---|---|---|---|---|---|
| TSH-00981 | CSH-TYP-001 | minimal valid contract: Semantic and ownership type vocabularies | minimum valid input named by contract | exact successful output and invariant; oracle: exhaustive enum behavior in consumers | unit | PR |
| TSH-00982 | CSH-TYP-001 | representative valid contract: Semantic and ownership type vocabularies | all ordinary fields and relationships | complete output and side effects match oracle; oracle: exhaustive enum behavior in consumers | unit | PR |
| TSH-00983 | CSH-TYP-001 | missing producer prerequisite: Semantic and ownership type vocabularies | otherwise valid input with one prerequisite absent | reject at owner boundary with no downstream publication; oracle: exhaustive enum behavior in consumers | unit | PR |
| TSH-00984 | CSH-TYP-001 | malformed relationship or value: Semantic and ownership type vocabularies | invalid ordering type layout token metadata or ownership relation | owned error and no accepted partial output; oracle: exhaustive enum behavior in consumers | unit | PR |
| TSH-00985 | CSH-TYP-001 | empty singleton duplicate and extrema: Semantic and ownership type vocabularies | empty one duplicate min max null and non-ASCII where applicable | documented boundary behavior for every applicable row; oracle: exhaustive enum behavior in consumers | unit | PR |
| TSH-00986 | CSH-TYP-001 | all named modes and variants: Semantic and ownership type vocabularies | every enum mode kind operator and ABI variant owned by contract | each variant preserves its named contract; oracle: exhaustive enum behavior in consumers | unit | PR |
| TSH-00987 | CSH-TYP-001 | repeatability and input-order perturbation: Semantic and ownership type vocabularies | same semantic input under repeated/permuted construction | identical semantic structure or bytes in canonical order; oracle: exhaustive enum behavior in consumers | unit | PR |
| TSH-00988 | CSH-TYP-002 | minimal valid contract: Semantic type value and layout | minimum valid input named by contract | exact successful output and invariant; oracle: all fields and transition table | unit | PR |
| TSH-00989 | CSH-TYP-002 | representative valid contract: Semantic type value and layout | all ordinary fields and relationships | complete output and side effects match oracle; oracle: all fields and transition table | unit | PR |
| TSH-00990 | CSH-TYP-002 | missing producer prerequisite: Semantic type value and layout | otherwise valid input with one prerequisite absent | reject at owner boundary with no downstream publication; oracle: all fields and transition table | unit | PR |
| TSH-00991 | CSH-TYP-002 | malformed relationship or value: Semantic type value and layout | invalid ordering type layout token metadata or ownership relation | owned error and no accepted partial output; oracle: all fields and transition table | unit | PR |
| TSH-00992 | CSH-TYP-002 | empty singleton duplicate and extrema: Semantic type value and layout | empty one duplicate min max null and non-ASCII where applicable | documented boundary behavior for every applicable row; oracle: all fields and transition table | unit | PR |
| TSH-00993 | CSH-TYP-002 | all named modes and variants: Semantic type value and layout | every enum mode kind operator and ABI variant owned by contract | each variant preserves its named contract; oracle: all fields and transition table | unit | PR |
| TSH-00994 | CSH-TYP-003 | minimal valid contract: Function parameter type list | minimum valid input named by contract | exact successful output and invariant; oracle: exact ordered IDs and count | unit | PR |
| TSH-00995 | CSH-TYP-003 | representative valid contract: Function parameter type list | all ordinary fields and relationships | complete output and side effects match oracle; oracle: exact ordered IDs and count | unit | PR |
| TSH-00996 | CSH-TYP-003 | missing producer prerequisite: Function parameter type list | otherwise valid input with one prerequisite absent | reject at owner boundary with no downstream publication; oracle: exact ordered IDs and count | unit | PR |
| TSH-00997 | CSH-TYP-003 | malformed relationship or value: Function parameter type list | invalid ordering type layout token metadata or ownership relation | owned error and no accepted partial output; oracle: exact ordered IDs and count | unit | PR |
| TSH-00998 | CSH-TYP-003 | empty singleton duplicate and extrema: Function parameter type list | empty one duplicate min max null and non-ASCII where applicable | documented boundary behavior for every applicable row; oracle: exact ordered IDs and count | unit | PR |
| TSH-00999 | CSH-TYP-003 | all named modes and variants: Function parameter type list | every enum mode kind operator and ABI variant owned by contract | each variant preserves its named contract; oracle: exact ordered IDs and count | unit | PR |
| TSH-01000 | CSH-TYP-003 | repeatability and input-order perturbation: Function parameter type list | same semantic input under repeated/permuted construction | identical semantic structure or bytes in canonical order; oracle: exact ordered IDs and count | unit | PR |
| TSH-01001 | CSH-TYP-004 | minimal valid contract: Builtin type table initialization | minimum valid input named by contract | exact successful output and invariant; oracle: complete builtin name/kind/layout inventory | unit | PR |
| TSH-01002 | CSH-TYP-004 | representative valid contract: Builtin type table initialization | all ordinary fields and relationships | complete output and side effects match oracle; oracle: complete builtin name/kind/layout inventory | unit | PR |
| TSH-01003 | CSH-TYP-004 | missing producer prerequisite: Builtin type table initialization | otherwise valid input with one prerequisite absent | reject at owner boundary with no downstream publication; oracle: complete builtin name/kind/layout inventory | unit | PR |
| TSH-01004 | CSH-TYP-004 | malformed relationship or value: Builtin type table initialization | invalid ordering type layout token metadata or ownership relation | owned error and no accepted partial output; oracle: complete builtin name/kind/layout inventory | unit | PR |
| TSH-01005 | CSH-TYP-004 | empty singleton duplicate and extrema: Builtin type table initialization | empty one duplicate min max null and non-ASCII where applicable | documented boundary behavior for every applicable row; oracle: complete builtin name/kind/layout inventory | unit | PR |
| TSH-01006 | CSH-TYP-004 | all named modes and variants: Builtin type table initialization | every enum mode kind operator and ABI variant owned by contract | each variant preserves its named contract; oracle: complete builtin name/kind/layout inventory | unit | PR |
| TSH-01007 | CSH-TYP-004 | repeatability and input-order perturbation: Builtin type table initialization | same semantic input under repeated/permuted construction | identical semantic structure or bytes in canonical order; oracle: complete builtin name/kind/layout inventory | unit | PR |
| TSH-01008 | CSH-TYP-005 | minimal valid contract: Type lookup and interning | minimum valid input named by contract | exact successful output and invariant; oracle: ID equality descriptors count and mutation stability | component | PR |
| TSH-01009 | CSH-TYP-005 | representative valid contract: Type lookup and interning | all ordinary fields and relationships | complete output and side effects match oracle; oracle: ID equality descriptors count and mutation stability | component | PR |
| TSH-01010 | CSH-TYP-005 | missing producer prerequisite: Type lookup and interning | otherwise valid input with one prerequisite absent | reject at owner boundary with no downstream publication; oracle: ID equality descriptors count and mutation stability | component | PR |
| TSH-01011 | CSH-TYP-005 | malformed relationship or value: Type lookup and interning | invalid ordering type layout token metadata or ownership relation | owned error and no accepted partial output; oracle: ID equality descriptors count and mutation stability | component | PR |
| TSH-01012 | CSH-TYP-005 | empty singleton duplicate and extrema: Type lookup and interning | empty one duplicate min max null and non-ASCII where applicable | documented boundary behavior for every applicable row; oracle: ID equality descriptors count and mutation stability | component | PR |
| TSH-01013 | CSH-TYP-005 | all named modes and variants: Type lookup and interning | every enum mode kind operator and ABI variant owned by contract | each variant preserves its named contract; oracle: ID equality descriptors count and mutation stability | component | PR |
| TSH-01014 | CSH-TYP-005 | failure after partial owned mutation: Type lookup and interning | valid prefix followed by one failing producer operation | no false accepted state and observable owner-consistent failure; oracle: ID equality descriptors count and mutation stability | component | NIGHTLY |
| TSH-01015 | CSH-TYP-006 | minimal valid contract: String literal type construction | minimum valid input named by contract | exact successful output and invariant; oracle: kind display element count and ID reuse | unit | PR |
| TSH-01016 | CSH-TYP-006 | representative valid contract: String literal type construction | all ordinary fields and relationships | complete output and side effects match oracle; oracle: kind display element count and ID reuse | unit | PR |
| TSH-01017 | CSH-TYP-006 | missing producer prerequisite: String literal type construction | otherwise valid input with one prerequisite absent | reject at owner boundary with no downstream publication; oracle: kind display element count and ID reuse | unit | PR |
| TSH-01018 | CSH-TYP-006 | malformed relationship or value: String literal type construction | invalid ordering type layout token metadata or ownership relation | owned error and no accepted partial output; oracle: kind display element count and ID reuse | unit | PR |
| TSH-01019 | CSH-TYP-006 | empty singleton duplicate and extrema: String literal type construction | empty one duplicate min max null and non-ASCII where applicable | documented boundary behavior for every applicable row; oracle: kind display element count and ID reuse | unit | PR |
| TSH-01020 | CSH-TYP-006 | all named modes and variants: String literal type construction | every enum mode kind operator and ABI variant owned by contract | each variant preserves its named contract; oracle: kind display element count and ID reuse | unit | PR |
| TSH-01021 | CSH-TYP-007 | minimal valid contract: Function type construction and numeric predicates | minimum valid input named by contract | exact successful output and invariant; oracle: signature descriptor and predicate truth table | unit | PR |
| TSH-01022 | CSH-TYP-007 | representative valid contract: Function type construction and numeric predicates | all ordinary fields and relationships | complete output and side effects match oracle; oracle: signature descriptor and predicate truth table | unit | PR |
| TSH-01023 | CSH-TYP-007 | missing producer prerequisite: Function type construction and numeric predicates | otherwise valid input with one prerequisite absent | reject at owner boundary with no downstream publication; oracle: signature descriptor and predicate truth table | unit | PR |
| TSH-01024 | CSH-TYP-007 | malformed relationship or value: Function type construction and numeric predicates | invalid ordering type layout token metadata or ownership relation | owned error and no accepted partial output; oracle: signature descriptor and predicate truth table | unit | PR |
| TSH-01025 | CSH-TYP-007 | empty singleton duplicate and extrema: Function type construction and numeric predicates | empty one duplicate min max null and non-ASCII where applicable | documented boundary behavior for every applicable row; oracle: signature descriptor and predicate truth table | unit | PR |
| TSH-01026 | CSH-TYP-007 | all named modes and variants: Function type construction and numeric predicates | every enum mode kind operator and ABI variant owned by contract | each variant preserves its named contract; oracle: signature descriptor and predicate truth table | unit | PR |
| TSH-01027 | CSH-TYP-007 | repeatability and input-order perturbation: Function type construction and numeric predicates | same semantic input under repeated/permuted construction | identical semantic structure or bytes in canonical order; oracle: signature descriptor and predicate truth table | unit | PR |

## 5. Gate de implementación futura

Cada caso deberá ejecutarse de forma individual, conservar evidencia y mapearse al contrato. El componente no podrá declararse cubierto por el simple éxito de un runner agregado. Coverage instrumental pertenece a la Fase 3.
