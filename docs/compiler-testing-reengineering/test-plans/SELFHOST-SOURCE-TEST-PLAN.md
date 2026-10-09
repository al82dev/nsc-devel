# Plan de pruebas — SelfHost.Source

## 1. Alcance

Plan desde cero para 8 contratos y 61 casos diseñados. No reutiliza el nombre histórico como oracle y no implementa ni ejecuta pruebas.

## 2. Estrategia futura

- Casos internos en un ejecutable unitario consolidado por componente, no un ejecutable por caso.
- Casos de lenguaje/proceso mediante el compilador construido y fixtures `.ns` externos mínimos.
- Oracles estructurales o semánticos; los mensajes sólo se comparan por código/categoría/campos estables.
- Self-host, escala y publicación costosa quedan `NIGHTLY`; los contratos pequeños y de frontera quedan `PR`.

## 3. Contratos

| Contrato | Nombre | Capa | Oracle |
|---|---|---|---|
| CSH-SRC-001 | Compile-time expression cursor and primitives | unit | cursor trace and consumed characters |
| CSH-SRC-002 | Compile-time primary and unary evaluation | unit | value validity and final cursor |
| CSH-SRC-003 | Compile-time precedence evaluation | component | value success and full-input consumption |
| CSH-SRC-004 | Conditional-source marker recognition | unit | truth table and cursor invariance |
| CSH-SRC-005 | Conditional-source transformation | component | output bytes line structure and diagnostic |
| CSH-SRC-006 | Diagnostic path normalization | unit | exact path vectors independent of cwd |
| CSH-SRC-007 | Source line-ending normalization and mapping | unit | text line-start table and exhaustive offset map |
| CSH-SRC-008 | Source loading identity and content hash | component | fields bytes hash line count and error |

## 4. Casos diseñados

| Caso | Contrato | Objetivo | Entrada | Esperado | Capa | Frecuencia |
|---|---|---|---|---|---|---|
| TSH-00849 | CSH-SRC-001 | minimal valid contract: Compile-time expression cursor and primitives | minimum valid input named by contract | exact successful output and invariant; oracle: cursor trace and consumed characters | unit | PR |
| TSH-00850 | CSH-SRC-001 | representative valid contract: Compile-time expression cursor and primitives | all ordinary fields and relationships | complete output and side effects match oracle; oracle: cursor trace and consumed characters | unit | PR |
| TSH-00851 | CSH-SRC-001 | missing producer prerequisite: Compile-time expression cursor and primitives | otherwise valid input with one prerequisite absent | reject at owner boundary with no downstream publication; oracle: cursor trace and consumed characters | unit | PR |
| TSH-00852 | CSH-SRC-001 | malformed relationship or value: Compile-time expression cursor and primitives | invalid ordering type layout token metadata or ownership relation | owned error and no accepted partial output; oracle: cursor trace and consumed characters | unit | PR |
| TSH-00853 | CSH-SRC-001 | empty singleton duplicate and extrema: Compile-time expression cursor and primitives | empty one duplicate min max null and non-ASCII where applicable | documented boundary behavior for every applicable row; oracle: cursor trace and consumed characters | unit | PR |
| TSH-00854 | CSH-SRC-001 | all named modes and variants: Compile-time expression cursor and primitives | every enum mode kind operator and ABI variant owned by contract | each variant preserves its named contract; oracle: cursor trace and consumed characters | unit | PR |
| TSH-00855 | CSH-SRC-001 | depth breadth and scale boundary: Compile-time expression cursor and primitives | large graph tree source or declaration set at documented limits | bounded completion or explicitly documented limitation without silent truncation; oracle: cursor trace and consumed characters | unit | NIGHTLY |
| TSH-00856 | CSH-SRC-002 | minimal valid contract: Compile-time primary and unary evaluation | minimum valid input named by contract | exact successful output and invariant; oracle: value validity and final cursor | unit | PR |
| TSH-00857 | CSH-SRC-002 | representative valid contract: Compile-time primary and unary evaluation | all ordinary fields and relationships | complete output and side effects match oracle; oracle: value validity and final cursor | unit | PR |
| TSH-00858 | CSH-SRC-002 | missing producer prerequisite: Compile-time primary and unary evaluation | otherwise valid input with one prerequisite absent | reject at owner boundary with no downstream publication; oracle: value validity and final cursor | unit | PR |
| TSH-00859 | CSH-SRC-002 | malformed relationship or value: Compile-time primary and unary evaluation | invalid ordering type layout token metadata or ownership relation | owned error and no accepted partial output; oracle: value validity and final cursor | unit | PR |
| TSH-00860 | CSH-SRC-002 | empty singleton duplicate and extrema: Compile-time primary and unary evaluation | empty one duplicate min max null and non-ASCII where applicable | documented boundary behavior for every applicable row; oracle: value validity and final cursor | unit | PR |
| TSH-00861 | CSH-SRC-002 | all named modes and variants: Compile-time primary and unary evaluation | every enum mode kind operator and ABI variant owned by contract | each variant preserves its named contract; oracle: value validity and final cursor | unit | PR |
| TSH-00862 | CSH-SRC-002 | repeatability and input-order perturbation: Compile-time primary and unary evaluation | same semantic input under repeated/permuted construction | identical semantic structure or bytes in canonical order; oracle: value validity and final cursor | unit | PR |
| TSH-00863 | CSH-SRC-002 | depth breadth and scale boundary: Compile-time primary and unary evaluation | large graph tree source or declaration set at documented limits | bounded completion or explicitly documented limitation without silent truncation; oracle: value validity and final cursor | unit | NIGHTLY |
| TSH-00864 | CSH-SRC-003 | minimal valid contract: Compile-time precedence evaluation | minimum valid input named by contract | exact successful output and invariant; oracle: value success and full-input consumption | component | PR |
| TSH-00865 | CSH-SRC-003 | representative valid contract: Compile-time precedence evaluation | all ordinary fields and relationships | complete output and side effects match oracle; oracle: value success and full-input consumption | component | PR |
| TSH-00866 | CSH-SRC-003 | missing producer prerequisite: Compile-time precedence evaluation | otherwise valid input with one prerequisite absent | reject at owner boundary with no downstream publication; oracle: value success and full-input consumption | component | PR |
| TSH-00867 | CSH-SRC-003 | malformed relationship or value: Compile-time precedence evaluation | invalid ordering type layout token metadata or ownership relation | owned error and no accepted partial output; oracle: value success and full-input consumption | component | PR |
| TSH-00868 | CSH-SRC-003 | empty singleton duplicate and extrema: Compile-time precedence evaluation | empty one duplicate min max null and non-ASCII where applicable | documented boundary behavior for every applicable row; oracle: value success and full-input consumption | component | PR |
| TSH-00869 | CSH-SRC-003 | all named modes and variants: Compile-time precedence evaluation | every enum mode kind operator and ABI variant owned by contract | each variant preserves its named contract; oracle: value success and full-input consumption | component | PR |
| TSH-00870 | CSH-SRC-003 | failure after partial owned mutation: Compile-time precedence evaluation | valid prefix followed by one failing producer operation | no false accepted state and observable owner-consistent failure; oracle: value success and full-input consumption | component | NIGHTLY |
| TSH-00871 | CSH-SRC-003 | depth breadth and scale boundary: Compile-time precedence evaluation | large graph tree source or declaration set at documented limits | bounded completion or explicitly documented limitation without silent truncation; oracle: value success and full-input consumption | component | NIGHTLY |
| TSH-00872 | CSH-SRC-004 | minimal valid contract: Conditional-source marker recognition | minimum valid input named by contract | exact successful output and invariant; oracle: truth table and cursor invariance | unit | PR |
| TSH-00873 | CSH-SRC-004 | representative valid contract: Conditional-source marker recognition | all ordinary fields and relationships | complete output and side effects match oracle; oracle: truth table and cursor invariance | unit | PR |
| TSH-00874 | CSH-SRC-004 | missing producer prerequisite: Conditional-source marker recognition | otherwise valid input with one prerequisite absent | reject at owner boundary with no downstream publication; oracle: truth table and cursor invariance | unit | PR |
| TSH-00875 | CSH-SRC-004 | malformed relationship or value: Conditional-source marker recognition | invalid ordering type layout token metadata or ownership relation | owned error and no accepted partial output; oracle: truth table and cursor invariance | unit | PR |
| TSH-00876 | CSH-SRC-004 | empty singleton duplicate and extrema: Conditional-source marker recognition | empty one duplicate min max null and non-ASCII where applicable | documented boundary behavior for every applicable row; oracle: truth table and cursor invariance | unit | PR |
| TSH-00877 | CSH-SRC-004 | all named modes and variants: Conditional-source marker recognition | every enum mode kind operator and ABI variant owned by contract | each variant preserves its named contract; oracle: truth table and cursor invariance | unit | PR |
| TSH-00878 | CSH-SRC-004 | depth breadth and scale boundary: Conditional-source marker recognition | large graph tree source or declaration set at documented limits | bounded completion or explicitly documented limitation without silent truncation; oracle: truth table and cursor invariance | unit | NIGHTLY |
| TSH-00879 | CSH-SRC-005 | minimal valid contract: Conditional-source transformation | minimum valid input named by contract | exact successful output and invariant; oracle: output bytes line structure and diagnostic | component | PR |
| TSH-00880 | CSH-SRC-005 | representative valid contract: Conditional-source transformation | all ordinary fields and relationships | complete output and side effects match oracle; oracle: output bytes line structure and diagnostic | component | PR |
| TSH-00881 | CSH-SRC-005 | missing producer prerequisite: Conditional-source transformation | otherwise valid input with one prerequisite absent | reject at owner boundary with no downstream publication; oracle: output bytes line structure and diagnostic | component | PR |
| TSH-00882 | CSH-SRC-005 | malformed relationship or value: Conditional-source transformation | invalid ordering type layout token metadata or ownership relation | owned error and no accepted partial output; oracle: output bytes line structure and diagnostic | component | PR |
| TSH-00883 | CSH-SRC-005 | empty singleton duplicate and extrema: Conditional-source transformation | empty one duplicate min max null and non-ASCII where applicable | documented boundary behavior for every applicable row; oracle: output bytes line structure and diagnostic | component | PR |
| TSH-00884 | CSH-SRC-005 | all named modes and variants: Conditional-source transformation | every enum mode kind operator and ABI variant owned by contract | each variant preserves its named contract; oracle: output bytes line structure and diagnostic | component | PR |
| TSH-00885 | CSH-SRC-005 | failure after partial owned mutation: Conditional-source transformation | valid prefix followed by one failing producer operation | no false accepted state and observable owner-consistent failure; oracle: output bytes line structure and diagnostic | component | NIGHTLY |
| TSH-00886 | CSH-SRC-005 | depth breadth and scale boundary: Conditional-source transformation | large graph tree source or declaration set at documented limits | bounded completion or explicitly documented limitation without silent truncation; oracle: output bytes line structure and diagnostic | component | NIGHTLY |
| TSH-00887 | CSH-SRC-006 | minimal valid contract: Diagnostic path normalization | minimum valid input named by contract | exact successful output and invariant; oracle: exact path vectors independent of cwd | unit | PR |
| TSH-00888 | CSH-SRC-006 | representative valid contract: Diagnostic path normalization | all ordinary fields and relationships | complete output and side effects match oracle; oracle: exact path vectors independent of cwd | unit | PR |
| TSH-00889 | CSH-SRC-006 | missing producer prerequisite: Diagnostic path normalization | otherwise valid input with one prerequisite absent | reject at owner boundary with no downstream publication; oracle: exact path vectors independent of cwd | unit | PR |
| TSH-00890 | CSH-SRC-006 | malformed relationship or value: Diagnostic path normalization | invalid ordering type layout token metadata or ownership relation | owned error and no accepted partial output; oracle: exact path vectors independent of cwd | unit | PR |
| TSH-00891 | CSH-SRC-006 | empty singleton duplicate and extrema: Diagnostic path normalization | empty one duplicate min max null and non-ASCII where applicable | documented boundary behavior for every applicable row; oracle: exact path vectors independent of cwd | unit | PR |
| TSH-00892 | CSH-SRC-006 | all named modes and variants: Diagnostic path normalization | every enum mode kind operator and ABI variant owned by contract | each variant preserves its named contract; oracle: exact path vectors independent of cwd | unit | PR |
| TSH-00893 | CSH-SRC-006 | repeatability and input-order perturbation: Diagnostic path normalization | same semantic input under repeated/permuted construction | identical semantic structure or bytes in canonical order; oracle: exact path vectors independent of cwd | unit | PR |
| TSH-00894 | CSH-SRC-007 | minimal valid contract: Source line-ending normalization and mapping | minimum valid input named by contract | exact successful output and invariant; oracle: text line-start table and exhaustive offset map | unit | PR |
| TSH-00895 | CSH-SRC-007 | representative valid contract: Source line-ending normalization and mapping | all ordinary fields and relationships | complete output and side effects match oracle; oracle: text line-start table and exhaustive offset map | unit | PR |
| TSH-00896 | CSH-SRC-007 | missing producer prerequisite: Source line-ending normalization and mapping | otherwise valid input with one prerequisite absent | reject at owner boundary with no downstream publication; oracle: text line-start table and exhaustive offset map | unit | PR |
| TSH-00897 | CSH-SRC-007 | malformed relationship or value: Source line-ending normalization and mapping | invalid ordering type layout token metadata or ownership relation | owned error and no accepted partial output; oracle: text line-start table and exhaustive offset map | unit | PR |
| TSH-00898 | CSH-SRC-007 | empty singleton duplicate and extrema: Source line-ending normalization and mapping | empty one duplicate min max null and non-ASCII where applicable | documented boundary behavior for every applicable row; oracle: text line-start table and exhaustive offset map | unit | PR |
| TSH-00899 | CSH-SRC-007 | all named modes and variants: Source line-ending normalization and mapping | every enum mode kind operator and ABI variant owned by contract | each variant preserves its named contract; oracle: text line-start table and exhaustive offset map | unit | PR |
| TSH-00900 | CSH-SRC-007 | depth breadth and scale boundary: Source line-ending normalization and mapping | large graph tree source or declaration set at documented limits | bounded completion or explicitly documented limitation without silent truncation; oracle: text line-start table and exhaustive offset map | unit | NIGHTLY |
| TSH-00901 | CSH-SRC-008 | minimal valid contract: Source loading identity and content hash | minimum valid input named by contract | exact successful output and invariant; oracle: fields bytes hash line count and error | component | PR |
| TSH-00902 | CSH-SRC-008 | representative valid contract: Source loading identity and content hash | all ordinary fields and relationships | complete output and side effects match oracle; oracle: fields bytes hash line count and error | component | PR |
| TSH-00903 | CSH-SRC-008 | missing producer prerequisite: Source loading identity and content hash | otherwise valid input with one prerequisite absent | reject at owner boundary with no downstream publication; oracle: fields bytes hash line count and error | component | PR |
| TSH-00904 | CSH-SRC-008 | malformed relationship or value: Source loading identity and content hash | invalid ordering type layout token metadata or ownership relation | owned error and no accepted partial output; oracle: fields bytes hash line count and error | component | PR |
| TSH-00905 | CSH-SRC-008 | empty singleton duplicate and extrema: Source loading identity and content hash | empty one duplicate min max null and non-ASCII where applicable | documented boundary behavior for every applicable row; oracle: fields bytes hash line count and error | component | PR |
| TSH-00906 | CSH-SRC-008 | all named modes and variants: Source loading identity and content hash | every enum mode kind operator and ABI variant owned by contract | each variant preserves its named contract; oracle: fields bytes hash line count and error | component | PR |
| TSH-00907 | CSH-SRC-008 | repeatability and input-order perturbation: Source loading identity and content hash | same semantic input under repeated/permuted construction | identical semantic structure or bytes in canonical order; oracle: fields bytes hash line count and error | component | PR |
| TSH-00908 | CSH-SRC-008 | failure after partial owned mutation: Source loading identity and content hash | valid prefix followed by one failing producer operation | no false accepted state and observable owner-consistent failure; oracle: fields bytes hash line count and error | component | NIGHTLY |
| TSH-00909 | CSH-SRC-008 | depth breadth and scale boundary: Source loading identity and content hash | large graph tree source or declaration set at documented limits | bounded completion or explicitly documented limitation without silent truncation; oracle: fields bytes hash line count and error | component | NIGHTLY |

## 5. Gate de implementación futura

Cada caso deberá ejecutarse de forma individual, conservar evidencia y mapearse al contrato. El componente no podrá declararse cubierto por el simple éxito de un runner agregado. Coverage instrumental pertenece a la Fase 3.
