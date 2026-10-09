# Plan de pruebas — SelfHost.Documentation

## 1. Alcance

Plan desde cero para 2 contratos y 12 casos diseñados. No reutiliza el nombre histórico como oracle y no implementa ni ejecuta pruebas.

## 2. Estrategia futura

- Casos internos en un ejecutable unitario consolidado por componente, no un ejecutable por caso.
- Casos de lenguaje/proceso mediante el compilador construido y fixtures `.ns` externos mínimos.
- Oracles estructurales o semánticos; los mensajes sólo se comparan por código/categoría/campos estables.
- Self-host, escala y publicación costosa quedan `NIGHTLY`; los contratos pequeños y de frontera quedan `PR`.

## 3. Contratos

| Contrato | Nombre | Capa | Oracle |
|---|---|---|---|
| CSH-DOC-001 | Self-host architecture declaration | build-contract | trace each claim to current source or mark historical |
| CSH-DOC-002 | Bootstrap profile and hidden-implementation prohibition | build-contract | source/project inventory and explicit contradiction record |

## 4. Casos diseñados

| Caso | Contrato | Objetivo | Entrada | Esperado | Capa | Frecuencia |
|---|---|---|---|---|---|---|
| TSH-00216 | CSH-DOC-001 | minimal valid contract: Self-host architecture declaration | minimum valid input named by contract | exact successful output and invariant; oracle: trace each claim to current source or mark historical | build-contract | PR |
| TSH-00217 | CSH-DOC-001 | representative valid contract: Self-host architecture declaration | all ordinary fields and relationships | complete output and side effects match oracle; oracle: trace each claim to current source or mark historical | build-contract | PR |
| TSH-00218 | CSH-DOC-001 | missing producer prerequisite: Self-host architecture declaration | otherwise valid input with one prerequisite absent | reject at owner boundary with no downstream publication; oracle: trace each claim to current source or mark historical | build-contract | PR |
| TSH-00219 | CSH-DOC-001 | malformed relationship or value: Self-host architecture declaration | invalid ordering type layout token metadata or ownership relation | owned error and no accepted partial output; oracle: trace each claim to current source or mark historical | build-contract | PR |
| TSH-00220 | CSH-DOC-001 | empty singleton duplicate and extrema: Self-host architecture declaration | empty one duplicate min max null and non-ASCII where applicable | documented boundary behavior for every applicable row; oracle: trace each claim to current source or mark historical | build-contract | PR |
| TSH-00221 | CSH-DOC-001 | all named modes and variants: Self-host architecture declaration | every enum mode kind operator and ABI variant owned by contract | each variant preserves its named contract; oracle: trace each claim to current source or mark historical | build-contract | PR |
| TSH-00222 | CSH-DOC-002 | minimal valid contract: Bootstrap profile and hidden-implementation prohibition | minimum valid input named by contract | exact successful output and invariant; oracle: source/project inventory and explicit contradiction record | build-contract | PR |
| TSH-00223 | CSH-DOC-002 | representative valid contract: Bootstrap profile and hidden-implementation prohibition | all ordinary fields and relationships | complete output and side effects match oracle; oracle: source/project inventory and explicit contradiction record | build-contract | PR |
| TSH-00224 | CSH-DOC-002 | missing producer prerequisite: Bootstrap profile and hidden-implementation prohibition | otherwise valid input with one prerequisite absent | reject at owner boundary with no downstream publication; oracle: source/project inventory and explicit contradiction record | build-contract | PR |
| TSH-00225 | CSH-DOC-002 | malformed relationship or value: Bootstrap profile and hidden-implementation prohibition | invalid ordering type layout token metadata or ownership relation | owned error and no accepted partial output; oracle: source/project inventory and explicit contradiction record | build-contract | PR |
| TSH-00226 | CSH-DOC-002 | empty singleton duplicate and extrema: Bootstrap profile and hidden-implementation prohibition | empty one duplicate min max null and non-ASCII where applicable | documented boundary behavior for every applicable row; oracle: source/project inventory and explicit contradiction record | build-contract | PR |
| TSH-00227 | CSH-DOC-002 | all named modes and variants: Bootstrap profile and hidden-implementation prohibition | every enum mode kind operator and ABI variant owned by contract | each variant preserves its named contract; oracle: source/project inventory and explicit contradiction record | build-contract | PR |

## 5. Gate de implementación futura

Cada caso deberá ejecutarse de forma individual, conservar evidencia y mapearse al contrato. El componente no podrá declararse cubierto por el simple éxito de un runner agregado. Coverage instrumental pertenece a la Fase 3.
