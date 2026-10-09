# Plan de pruebas — SelfHost.Bootstrap

## 1. Alcance

Plan desde cero para 3 contratos y 23 casos diseñados. No reutiliza el nombre histórico como oracle y no implementa ni ejecuta pruebas.

## 2. Estrategia futura

- Casos internos en un ejecutable unitario consolidado por componente, no un ejecutable por caso.
- Casos de lenguaje/proceso mediante el compilador construido y fixtures `.ns` externos mínimos.
- Oracles estructurales o semánticos; los mensajes sólo se comparan por código/categoría/campos estables.
- Self-host, escala y publicación costosa quedan `NIGHTLY`; los contratos pequeños y de frontera quedan `PR`.

## 3. Contratos

| Contrato | Nombre | Capa | Oracle |
|---|---|---|---|
| CSH-BOO-001 | Bootstrap source profile verification | component | accept/reject matrix with first rejected construct |
| CSH-BOO-002 | Bootstrap manifest verification | component | normalized manifest model or exact rejection category |
| CSH-BOO-003 | Bootstrap generation build orchestration | integration | stage sequence outputs diagnostics and exit result |

## 4. Casos diseñados

| Caso | Contrato | Objetivo | Entrada | Esperado | Capa | Frecuencia |
|---|---|---|---|---|---|---|
| TSH-00029 | CSH-BOO-001 | minimal valid contract: Bootstrap source profile verification | minimum valid input named by contract | exact successful output and invariant; oracle: accept/reject matrix with first rejected construct | component | PR |
| TSH-00030 | CSH-BOO-001 | representative valid contract: Bootstrap source profile verification | all ordinary fields and relationships | complete output and side effects match oracle; oracle: accept/reject matrix with first rejected construct | component | PR |
| TSH-00031 | CSH-BOO-001 | missing producer prerequisite: Bootstrap source profile verification | otherwise valid input with one prerequisite absent | reject at owner boundary with no downstream publication; oracle: accept/reject matrix with first rejected construct | component | PR |
| TSH-00032 | CSH-BOO-001 | malformed relationship or value: Bootstrap source profile verification | invalid ordering type layout token metadata or ownership relation | owned error and no accepted partial output; oracle: accept/reject matrix with first rejected construct | component | PR |
| TSH-00033 | CSH-BOO-001 | empty singleton duplicate and extrema: Bootstrap source profile verification | empty one duplicate min max null and non-ASCII where applicable | documented boundary behavior for every applicable row; oracle: accept/reject matrix with first rejected construct | component | PR |
| TSH-00034 | CSH-BOO-001 | all named modes and variants: Bootstrap source profile verification | every enum mode kind operator and ABI variant owned by contract | each variant preserves its named contract; oracle: accept/reject matrix with first rejected construct | component | PR |
| TSH-00035 | CSH-BOO-001 | failure after partial owned mutation: Bootstrap source profile verification | valid prefix followed by one failing producer operation | no false accepted state and observable owner-consistent failure; oracle: accept/reject matrix with first rejected construct | component | NIGHTLY |
| TSH-00036 | CSH-BOO-001 | depth breadth and scale boundary: Bootstrap source profile verification | large graph tree source or declaration set at documented limits | bounded completion or explicitly documented limitation without silent truncation; oracle: accept/reject matrix with first rejected construct | component | NIGHTLY |
| TSH-00037 | CSH-BOO-002 | minimal valid contract: Bootstrap manifest verification | minimum valid input named by contract | exact successful output and invariant; oracle: normalized manifest model or exact rejection category | component | PR |
| TSH-00038 | CSH-BOO-002 | representative valid contract: Bootstrap manifest verification | all ordinary fields and relationships | complete output and side effects match oracle; oracle: normalized manifest model or exact rejection category | component | PR |
| TSH-00039 | CSH-BOO-002 | missing producer prerequisite: Bootstrap manifest verification | otherwise valid input with one prerequisite absent | reject at owner boundary with no downstream publication; oracle: normalized manifest model or exact rejection category | component | PR |
| TSH-00040 | CSH-BOO-002 | malformed relationship or value: Bootstrap manifest verification | invalid ordering type layout token metadata or ownership relation | owned error and no accepted partial output; oracle: normalized manifest model or exact rejection category | component | PR |
| TSH-00041 | CSH-BOO-002 | empty singleton duplicate and extrema: Bootstrap manifest verification | empty one duplicate min max null and non-ASCII where applicable | documented boundary behavior for every applicable row; oracle: normalized manifest model or exact rejection category | component | PR |
| TSH-00042 | CSH-BOO-002 | all named modes and variants: Bootstrap manifest verification | every enum mode kind operator and ABI variant owned by contract | each variant preserves its named contract; oracle: normalized manifest model or exact rejection category | component | PR |
| TSH-00043 | CSH-BOO-002 | failure after partial owned mutation: Bootstrap manifest verification | valid prefix followed by one failing producer operation | no false accepted state and observable owner-consistent failure; oracle: normalized manifest model or exact rejection category | component | NIGHTLY |
| TSH-00044 | CSH-BOO-002 | depth breadth and scale boundary: Bootstrap manifest verification | large graph tree source or declaration set at documented limits | bounded completion or explicitly documented limitation without silent truncation; oracle: normalized manifest model or exact rejection category | component | NIGHTLY |
| TSH-00045 | CSH-BOO-003 | minimal valid contract: Bootstrap generation build orchestration | minimum valid input named by contract | exact successful output and invariant; oracle: stage sequence outputs diagnostics and exit result | integration | PR |
| TSH-00046 | CSH-BOO-003 | representative valid contract: Bootstrap generation build orchestration | all ordinary fields and relationships | complete output and side effects match oracle; oracle: stage sequence outputs diagnostics and exit result | integration | PR |
| TSH-00047 | CSH-BOO-003 | missing producer prerequisite: Bootstrap generation build orchestration | otherwise valid input with one prerequisite absent | reject at owner boundary with no downstream publication; oracle: stage sequence outputs diagnostics and exit result | integration | PR |
| TSH-00048 | CSH-BOO-003 | malformed relationship or value: Bootstrap generation build orchestration | invalid ordering type layout token metadata or ownership relation | owned error and no accepted partial output; oracle: stage sequence outputs diagnostics and exit result | integration | PR |
| TSH-00049 | CSH-BOO-003 | empty singleton duplicate and extrema: Bootstrap generation build orchestration | empty one duplicate min max null and non-ASCII where applicable | documented boundary behavior for every applicable row; oracle: stage sequence outputs diagnostics and exit result | integration | PR |
| TSH-00050 | CSH-BOO-003 | all named modes and variants: Bootstrap generation build orchestration | every enum mode kind operator and ABI variant owned by contract | each variant preserves its named contract; oracle: stage sequence outputs diagnostics and exit result | integration | PR |
| TSH-00051 | CSH-BOO-003 | failure after partial owned mutation: Bootstrap generation build orchestration | valid prefix followed by one failing producer operation | no false accepted state and observable owner-consistent failure; oracle: stage sequence outputs diagnostics and exit result | integration | NIGHTLY |

## 5. Gate de implementación futura

Cada caso deberá ejecutarse de forma individual, conservar evidencia y mapearse al contrato. El componente no podrá declararse cubierto por el simple éxito de un runner agregado. Coverage instrumental pertenece a la Fase 3.
