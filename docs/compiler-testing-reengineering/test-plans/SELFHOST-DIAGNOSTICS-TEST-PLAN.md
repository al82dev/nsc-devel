# Plan de pruebas — SelfHost.Diagnostics

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
| CSH-DIA-001 | Diagnostic severity and immutable record | unit | exact field values and copy lifetime |
| CSH-DIA-002 | Diagnostic catalog | unit | exact code severity argument and exit code |
| CSH-DIA-003 | Human diagnostic rendering | unit | exact normalized semantic fields ignoring incidental locale |
| CSH-DIA-004 | JSON diagnostic rendering | unit | parse JSON then compare exact typed fields and escaping |

## 4. Casos diseñados

| Caso | Contrato | Objetivo | Entrada | Esperado | Capa | Frecuencia |
|---|---|---|---|---|---|---|
| TSH-00188 | CSH-DIA-001 | minimal valid contract: Diagnostic severity and immutable record | minimum valid input named by contract | exact successful output and invariant; oracle: exact field values and copy lifetime | unit | PR |
| TSH-00189 | CSH-DIA-001 | representative valid contract: Diagnostic severity and immutable record | all ordinary fields and relationships | complete output and side effects match oracle; oracle: exact field values and copy lifetime | unit | PR |
| TSH-00190 | CSH-DIA-001 | missing producer prerequisite: Diagnostic severity and immutable record | otherwise valid input with one prerequisite absent | reject at owner boundary with no downstream publication; oracle: exact field values and copy lifetime | unit | PR |
| TSH-00191 | CSH-DIA-001 | malformed relationship or value: Diagnostic severity and immutable record | invalid ordering type layout token metadata or ownership relation | owned error and no accepted partial output; oracle: exact field values and copy lifetime | unit | PR |
| TSH-00192 | CSH-DIA-001 | empty singleton duplicate and extrema: Diagnostic severity and immutable record | empty one duplicate min max null and non-ASCII where applicable | documented boundary behavior for every applicable row; oracle: exact field values and copy lifetime | unit | PR |
| TSH-00193 | CSH-DIA-001 | all named modes and variants: Diagnostic severity and immutable record | every enum mode kind operator and ABI variant owned by contract | each variant preserves its named contract; oracle: exact field values and copy lifetime | unit | PR |
| TSH-00194 | CSH-DIA-001 | repeatability and input-order perturbation: Diagnostic severity and immutable record | same semantic input under repeated/permuted construction | identical semantic structure or bytes in canonical order; oracle: exact field values and copy lifetime | unit | PR |
| TSH-00195 | CSH-DIA-002 | minimal valid contract: Diagnostic catalog | minimum valid input named by contract | exact successful output and invariant; oracle: exact code severity argument and exit code | unit | PR |
| TSH-00196 | CSH-DIA-002 | representative valid contract: Diagnostic catalog | all ordinary fields and relationships | complete output and side effects match oracle; oracle: exact code severity argument and exit code | unit | PR |
| TSH-00197 | CSH-DIA-002 | missing producer prerequisite: Diagnostic catalog | otherwise valid input with one prerequisite absent | reject at owner boundary with no downstream publication; oracle: exact code severity argument and exit code | unit | PR |
| TSH-00198 | CSH-DIA-002 | malformed relationship or value: Diagnostic catalog | invalid ordering type layout token metadata or ownership relation | owned error and no accepted partial output; oracle: exact code severity argument and exit code | unit | PR |
| TSH-00199 | CSH-DIA-002 | empty singleton duplicate and extrema: Diagnostic catalog | empty one duplicate min max null and non-ASCII where applicable | documented boundary behavior for every applicable row; oracle: exact code severity argument and exit code | unit | PR |
| TSH-00200 | CSH-DIA-002 | all named modes and variants: Diagnostic catalog | every enum mode kind operator and ABI variant owned by contract | each variant preserves its named contract; oracle: exact code severity argument and exit code | unit | PR |
| TSH-00201 | CSH-DIA-002 | repeatability and input-order perturbation: Diagnostic catalog | same semantic input under repeated/permuted construction | identical semantic structure or bytes in canonical order; oracle: exact code severity argument and exit code | unit | PR |
| TSH-00202 | CSH-DIA-003 | minimal valid contract: Human diagnostic rendering | minimum valid input named by contract | exact successful output and invariant; oracle: exact normalized semantic fields ignoring incidental locale | unit | PR |
| TSH-00203 | CSH-DIA-003 | representative valid contract: Human diagnostic rendering | all ordinary fields and relationships | complete output and side effects match oracle; oracle: exact normalized semantic fields ignoring incidental locale | unit | PR |
| TSH-00204 | CSH-DIA-003 | missing producer prerequisite: Human diagnostic rendering | otherwise valid input with one prerequisite absent | reject at owner boundary with no downstream publication; oracle: exact normalized semantic fields ignoring incidental locale | unit | PR |
| TSH-00205 | CSH-DIA-003 | malformed relationship or value: Human diagnostic rendering | invalid ordering type layout token metadata or ownership relation | owned error and no accepted partial output; oracle: exact normalized semantic fields ignoring incidental locale | unit | PR |
| TSH-00206 | CSH-DIA-003 | empty singleton duplicate and extrema: Human diagnostic rendering | empty one duplicate min max null and non-ASCII where applicable | documented boundary behavior for every applicable row; oracle: exact normalized semantic fields ignoring incidental locale | unit | PR |
| TSH-00207 | CSH-DIA-003 | all named modes and variants: Human diagnostic rendering | every enum mode kind operator and ABI variant owned by contract | each variant preserves its named contract; oracle: exact normalized semantic fields ignoring incidental locale | unit | PR |
| TSH-00208 | CSH-DIA-003 | repeatability and input-order perturbation: Human diagnostic rendering | same semantic input under repeated/permuted construction | identical semantic structure or bytes in canonical order; oracle: exact normalized semantic fields ignoring incidental locale | unit | PR |
| TSH-00209 | CSH-DIA-004 | minimal valid contract: JSON diagnostic rendering | minimum valid input named by contract | exact successful output and invariant; oracle: parse JSON then compare exact typed fields and escaping | unit | PR |
| TSH-00210 | CSH-DIA-004 | representative valid contract: JSON diagnostic rendering | all ordinary fields and relationships | complete output and side effects match oracle; oracle: parse JSON then compare exact typed fields and escaping | unit | PR |
| TSH-00211 | CSH-DIA-004 | missing producer prerequisite: JSON diagnostic rendering | otherwise valid input with one prerequisite absent | reject at owner boundary with no downstream publication; oracle: parse JSON then compare exact typed fields and escaping | unit | PR |
| TSH-00212 | CSH-DIA-004 | malformed relationship or value: JSON diagnostic rendering | invalid ordering type layout token metadata or ownership relation | owned error and no accepted partial output; oracle: parse JSON then compare exact typed fields and escaping | unit | PR |
| TSH-00213 | CSH-DIA-004 | empty singleton duplicate and extrema: JSON diagnostic rendering | empty one duplicate min max null and non-ASCII where applicable | documented boundary behavior for every applicable row; oracle: parse JSON then compare exact typed fields and escaping | unit | PR |
| TSH-00214 | CSH-DIA-004 | all named modes and variants: JSON diagnostic rendering | every enum mode kind operator and ABI variant owned by contract | each variant preserves its named contract; oracle: parse JSON then compare exact typed fields and escaping | unit | PR |
| TSH-00215 | CSH-DIA-004 | repeatability and input-order perturbation: JSON diagnostic rendering | same semantic input under repeated/permuted construction | identical semantic structure or bytes in canonical order; oracle: parse JSON then compare exact typed fields and escaping | unit | PR |

## 5. Gate de implementación futura

Cada caso deberá ejecutarse de forma individual, conservar evidencia y mapearse al contrato. El componente no podrá declararse cubierto por el simple éxito de un runner agregado. Coverage instrumental pertenece a la Fase 3.
