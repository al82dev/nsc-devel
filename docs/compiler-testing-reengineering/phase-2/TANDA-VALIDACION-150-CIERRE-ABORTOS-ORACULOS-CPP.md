# Validación 150 — cierre de abortos en oráculos C++

## Alcance

Esta tanda corrige exclusivamente cuatro continuaciones inseguras de los
oráculos de la suite nueva. No modifica NSC, semántica del lenguaje, contratos,
manifiestos ni resultados esperados.

## Causas confirmadas

### Frontend — escalera de nombres

`namespaceOracle()` registraba que la escalera estaba vacía y continuaba con
`ladder.front()`. El caso `TF-FRONT-043` terminaba en la aserción del runtime de
MSVC antes de publicar el `FAIL` contractual.

### Frontend — expansión de macros

`macroOracle()` registraba que no existía exactamente una expansión y después
usaba `first.expansions.front()`. Los siete casos aplicables de
`CF-FRONT-052` abortaban por la misma continuación.

### HIR — propiedad del árbol

`lowerCase()` devolvía un `HirNode*` perteneciente al `FrontendCompilation`
local. Al retornar destruía al propietario y los ocho casos de `CF-HIR-011`
desreferenciaban almacenamiento liberado. El oráculo conserva ahora el
`FrontendCompilation` durante toda la inspección.

### MIR — topología de bloques

`blockTopologyOracle()` registraba que faltaba `Main` o que no tenía bloques,
pero iteraba inmediatamente `function->blocks()`. Los dos fixtures que no
produjeron una función utilizable terminaban con acceso inválido.

## Contrato preservado

Las precondiciones conservan los mismos mensajes y la misma expectativa. La
única diferencia es que una precondición incumplida termina el oráculo actual y
permite que el host publique un resultado `FAIL` normal. Ningún fallo se
convierte en `PASS` y no se adapta el compilador a la prueba.

## Validación dirigida

Se construyó únicamente `nsc_component_tests`: `6/6` pasos terminaron sin
errores. Después se ejecutaron los `18` casos identificados por la corrida
`20261010-090333-82a4a13f`.

- `18/18` procesos terminaron sin aserción ni acceso inválido;
- `18/18` casos publicaron exactamente un sobre JSON;
- los `8` casos HIR terminaron `PASS`;
- los `8` casos Frontend y los `2` casos MIR terminaron `FAIL` con evidencia
  contractual ordinaria;
- quedaron `0` resultados de infraestructura en este conjunto dirigido.

No se ejecuta la batería completa.
