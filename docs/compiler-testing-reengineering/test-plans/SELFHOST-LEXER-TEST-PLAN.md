# Plan de pruebas — SelfHost.Lexer

## 1. Alcance

Plan desde cero para 8 contratos y 53 casos diseñados. No reutiliza el nombre histórico como oracle y no implementa ni ejecuta pruebas.

## 2. Estrategia futura

- Casos internos en un ejecutable unitario consolidado por componente, no un ejecutable por caso.
- Casos de lenguaje/proceso mediante el compilador construido y fixtures `.ns` externos mínimos.
- Oracles estructurales o semánticos; los mensajes sólo se comparan por código/categoría/campos estables.
- Self-host, escala y publicación costosa quedan `NIGHTLY`; los contratos pequeños y de frontera quedan `PR`.

## 3. Contratos

| Contrato | Nombre | Capa | Oracle |
|---|---|---|---|
| CSH-LEX-001 | Token vocabulary and stable names | unit | exhaustive enum/name table |
| CSH-LEX-002 | Token stream storage and error count | unit | tokens ranges order and error count |
| CSH-LEX-003 | Cursor and character classification | unit | cursor trace and classification truth table |
| CSH-LEX-004 | Trivia and comments | unit | final cursor error count and next token |
| CSH-LEX-005 | Identifiers and keywords | unit | kind lexeme and range |
| CSH-LEX-006 | Numeric literals | unit | token kind text range and next cursor |
| CSH-LEX-007 | Character string raw and interpolated literals | unit | decoded value token sequence ranges and error count |
| CSH-LEX-008 | Punctuation inline assembly and full lexing | component | token sequence EOF range dump and errors |

## 4. Casos diseñados

| Caso | Contrato | Objetivo | Entrada | Esperado | Capa | Frecuencia |
|---|---|---|---|---|---|---|
| TSH-00399 | CSH-LEX-001 | minimal valid contract: Token vocabulary and stable names | minimum valid input named by contract | exact successful output and invariant; oracle: exhaustive enum/name table | unit | PR |
| TSH-00400 | CSH-LEX-001 | representative valid contract: Token vocabulary and stable names | all ordinary fields and relationships | complete output and side effects match oracle; oracle: exhaustive enum/name table | unit | PR |
| TSH-00401 | CSH-LEX-001 | missing producer prerequisite: Token vocabulary and stable names | otherwise valid input with one prerequisite absent | reject at owner boundary with no downstream publication; oracle: exhaustive enum/name table | unit | PR |
| TSH-00402 | CSH-LEX-001 | malformed relationship or value: Token vocabulary and stable names | invalid ordering type layout token metadata or ownership relation | owned error and no accepted partial output; oracle: exhaustive enum/name table | unit | PR |
| TSH-00403 | CSH-LEX-001 | empty singleton duplicate and extrema: Token vocabulary and stable names | empty one duplicate min max null and non-ASCII where applicable | documented boundary behavior for every applicable row; oracle: exhaustive enum/name table | unit | PR |
| TSH-00404 | CSH-LEX-001 | all named modes and variants: Token vocabulary and stable names | every enum mode kind operator and ABI variant owned by contract | each variant preserves its named contract; oracle: exhaustive enum/name table | unit | PR |
| TSH-00405 | CSH-LEX-001 | repeatability and input-order perturbation: Token vocabulary and stable names | same semantic input under repeated/permuted construction | identical semantic structure or bytes in canonical order; oracle: exhaustive enum/name table | unit | PR |
| TSH-00406 | CSH-LEX-002 | minimal valid contract: Token stream storage and error count | minimum valid input named by contract | exact successful output and invariant; oracle: tokens ranges order and error count | unit | PR |
| TSH-00407 | CSH-LEX-002 | representative valid contract: Token stream storage and error count | all ordinary fields and relationships | complete output and side effects match oracle; oracle: tokens ranges order and error count | unit | PR |
| TSH-00408 | CSH-LEX-002 | missing producer prerequisite: Token stream storage and error count | otherwise valid input with one prerequisite absent | reject at owner boundary with no downstream publication; oracle: tokens ranges order and error count | unit | PR |
| TSH-00409 | CSH-LEX-002 | malformed relationship or value: Token stream storage and error count | invalid ordering type layout token metadata or ownership relation | owned error and no accepted partial output; oracle: tokens ranges order and error count | unit | PR |
| TSH-00410 | CSH-LEX-002 | empty singleton duplicate and extrema: Token stream storage and error count | empty one duplicate min max null and non-ASCII where applicable | documented boundary behavior for every applicable row; oracle: tokens ranges order and error count | unit | PR |
| TSH-00411 | CSH-LEX-002 | all named modes and variants: Token stream storage and error count | every enum mode kind operator and ABI variant owned by contract | each variant preserves its named contract; oracle: tokens ranges order and error count | unit | PR |
| TSH-00412 | CSH-LEX-002 | repeatability and input-order perturbation: Token stream storage and error count | same semantic input under repeated/permuted construction | identical semantic structure or bytes in canonical order; oracle: tokens ranges order and error count | unit | PR |
| TSH-00413 | CSH-LEX-003 | minimal valid contract: Cursor and character classification | minimum valid input named by contract | exact successful output and invariant; oracle: cursor trace and classification truth table | unit | PR |
| TSH-00414 | CSH-LEX-003 | representative valid contract: Cursor and character classification | all ordinary fields and relationships | complete output and side effects match oracle; oracle: cursor trace and classification truth table | unit | PR |
| TSH-00415 | CSH-LEX-003 | missing producer prerequisite: Cursor and character classification | otherwise valid input with one prerequisite absent | reject at owner boundary with no downstream publication; oracle: cursor trace and classification truth table | unit | PR |
| TSH-00416 | CSH-LEX-003 | malformed relationship or value: Cursor and character classification | invalid ordering type layout token metadata or ownership relation | owned error and no accepted partial output; oracle: cursor trace and classification truth table | unit | PR |
| TSH-00417 | CSH-LEX-003 | empty singleton duplicate and extrema: Cursor and character classification | empty one duplicate min max null and non-ASCII where applicable | documented boundary behavior for every applicable row; oracle: cursor trace and classification truth table | unit | PR |
| TSH-00418 | CSH-LEX-003 | all named modes and variants: Cursor and character classification | every enum mode kind operator and ABI variant owned by contract | each variant preserves its named contract; oracle: cursor trace and classification truth table | unit | PR |
| TSH-00419 | CSH-LEX-003 | repeatability and input-order perturbation: Cursor and character classification | same semantic input under repeated/permuted construction | identical semantic structure or bytes in canonical order; oracle: cursor trace and classification truth table | unit | PR |
| TSH-00420 | CSH-LEX-004 | minimal valid contract: Trivia and comments | minimum valid input named by contract | exact successful output and invariant; oracle: final cursor error count and next token | unit | PR |
| TSH-00421 | CSH-LEX-004 | representative valid contract: Trivia and comments | all ordinary fields and relationships | complete output and side effects match oracle; oracle: final cursor error count and next token | unit | PR |
| TSH-00422 | CSH-LEX-004 | missing producer prerequisite: Trivia and comments | otherwise valid input with one prerequisite absent | reject at owner boundary with no downstream publication; oracle: final cursor error count and next token | unit | PR |
| TSH-00423 | CSH-LEX-004 | malformed relationship or value: Trivia and comments | invalid ordering type layout token metadata or ownership relation | owned error and no accepted partial output; oracle: final cursor error count and next token | unit | PR |
| TSH-00424 | CSH-LEX-004 | empty singleton duplicate and extrema: Trivia and comments | empty one duplicate min max null and non-ASCII where applicable | documented boundary behavior for every applicable row; oracle: final cursor error count and next token | unit | PR |
| TSH-00425 | CSH-LEX-004 | all named modes and variants: Trivia and comments | every enum mode kind operator and ABI variant owned by contract | each variant preserves its named contract; oracle: final cursor error count and next token | unit | PR |
| TSH-00426 | CSH-LEX-005 | minimal valid contract: Identifiers and keywords | minimum valid input named by contract | exact successful output and invariant; oracle: kind lexeme and range | unit | PR |
| TSH-00427 | CSH-LEX-005 | representative valid contract: Identifiers and keywords | all ordinary fields and relationships | complete output and side effects match oracle; oracle: kind lexeme and range | unit | PR |
| TSH-00428 | CSH-LEX-005 | missing producer prerequisite: Identifiers and keywords | otherwise valid input with one prerequisite absent | reject at owner boundary with no downstream publication; oracle: kind lexeme and range | unit | PR |
| TSH-00429 | CSH-LEX-005 | malformed relationship or value: Identifiers and keywords | invalid ordering type layout token metadata or ownership relation | owned error and no accepted partial output; oracle: kind lexeme and range | unit | PR |
| TSH-00430 | CSH-LEX-005 | empty singleton duplicate and extrema: Identifiers and keywords | empty one duplicate min max null and non-ASCII where applicable | documented boundary behavior for every applicable row; oracle: kind lexeme and range | unit | PR |
| TSH-00431 | CSH-LEX-005 | all named modes and variants: Identifiers and keywords | every enum mode kind operator and ABI variant owned by contract | each variant preserves its named contract; oracle: kind lexeme and range | unit | PR |
| TSH-00432 | CSH-LEX-006 | minimal valid contract: Numeric literals | minimum valid input named by contract | exact successful output and invariant; oracle: token kind text range and next cursor | unit | PR |
| TSH-00433 | CSH-LEX-006 | representative valid contract: Numeric literals | all ordinary fields and relationships | complete output and side effects match oracle; oracle: token kind text range and next cursor | unit | PR |
| TSH-00434 | CSH-LEX-006 | missing producer prerequisite: Numeric literals | otherwise valid input with one prerequisite absent | reject at owner boundary with no downstream publication; oracle: token kind text range and next cursor | unit | PR |
| TSH-00435 | CSH-LEX-006 | malformed relationship or value: Numeric literals | invalid ordering type layout token metadata or ownership relation | owned error and no accepted partial output; oracle: token kind text range and next cursor | unit | PR |
| TSH-00436 | CSH-LEX-006 | empty singleton duplicate and extrema: Numeric literals | empty one duplicate min max null and non-ASCII where applicable | documented boundary behavior for every applicable row; oracle: token kind text range and next cursor | unit | PR |
| TSH-00437 | CSH-LEX-006 | all named modes and variants: Numeric literals | every enum mode kind operator and ABI variant owned by contract | each variant preserves its named contract; oracle: token kind text range and next cursor | unit | PR |
| TSH-00438 | CSH-LEX-007 | minimal valid contract: Character string raw and interpolated literals | minimum valid input named by contract | exact successful output and invariant; oracle: decoded value token sequence ranges and error count | unit | PR |
| TSH-00439 | CSH-LEX-007 | representative valid contract: Character string raw and interpolated literals | all ordinary fields and relationships | complete output and side effects match oracle; oracle: decoded value token sequence ranges and error count | unit | PR |
| TSH-00440 | CSH-LEX-007 | missing producer prerequisite: Character string raw and interpolated literals | otherwise valid input with one prerequisite absent | reject at owner boundary with no downstream publication; oracle: decoded value token sequence ranges and error count | unit | PR |
| TSH-00441 | CSH-LEX-007 | malformed relationship or value: Character string raw and interpolated literals | invalid ordering type layout token metadata or ownership relation | owned error and no accepted partial output; oracle: decoded value token sequence ranges and error count | unit | PR |
| TSH-00442 | CSH-LEX-007 | empty singleton duplicate and extrema: Character string raw and interpolated literals | empty one duplicate min max null and non-ASCII where applicable | documented boundary behavior for every applicable row; oracle: decoded value token sequence ranges and error count | unit | PR |
| TSH-00443 | CSH-LEX-007 | all named modes and variants: Character string raw and interpolated literals | every enum mode kind operator and ABI variant owned by contract | each variant preserves its named contract; oracle: decoded value token sequence ranges and error count | unit | PR |
| TSH-00444 | CSH-LEX-008 | minimal valid contract: Punctuation inline assembly and full lexing | minimum valid input named by contract | exact successful output and invariant; oracle: token sequence EOF range dump and errors | component | PR |
| TSH-00445 | CSH-LEX-008 | representative valid contract: Punctuation inline assembly and full lexing | all ordinary fields and relationships | complete output and side effects match oracle; oracle: token sequence EOF range dump and errors | component | PR |
| TSH-00446 | CSH-LEX-008 | missing producer prerequisite: Punctuation inline assembly and full lexing | otherwise valid input with one prerequisite absent | reject at owner boundary with no downstream publication; oracle: token sequence EOF range dump and errors | component | PR |
| TSH-00447 | CSH-LEX-008 | malformed relationship or value: Punctuation inline assembly and full lexing | invalid ordering type layout token metadata or ownership relation | owned error and no accepted partial output; oracle: token sequence EOF range dump and errors | component | PR |
| TSH-00448 | CSH-LEX-008 | empty singleton duplicate and extrema: Punctuation inline assembly and full lexing | empty one duplicate min max null and non-ASCII where applicable | documented boundary behavior for every applicable row; oracle: token sequence EOF range dump and errors | component | PR |
| TSH-00449 | CSH-LEX-008 | all named modes and variants: Punctuation inline assembly and full lexing | every enum mode kind operator and ABI variant owned by contract | each variant preserves its named contract; oracle: token sequence EOF range dump and errors | component | PR |
| TSH-00450 | CSH-LEX-008 | repeatability and input-order perturbation: Punctuation inline assembly and full lexing | same semantic input under repeated/permuted construction | identical semantic structure or bytes in canonical order; oracle: token sequence EOF range dump and errors | component | PR |
| TSH-00451 | CSH-LEX-008 | failure after partial owned mutation: Punctuation inline assembly and full lexing | valid prefix followed by one failing producer operation | no false accepted state and observable owner-consistent failure; oracle: token sequence EOF range dump and errors | component | NIGHTLY |

## 5. Gate de implementación futura

Cada caso deberá ejecutarse de forma individual, conservar evidencia y mapearse al contrato. El componente no podrá declararse cubierto por el simple éxito de un runner agregado. Coverage instrumental pertenece a la Fase 3.
