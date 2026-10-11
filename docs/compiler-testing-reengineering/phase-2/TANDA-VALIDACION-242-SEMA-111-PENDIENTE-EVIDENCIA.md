# Tanda de validación 242 — TS-SEMA-111 pendiente de evidencia

Fecha: 2026-10-10

## Estado

PENDIENTE_EVIDENCIA. SIN CAMBIO DE CÓDIGO.

## Evidencia disponible

La corrida `20261010-142113-a514d35c` conserva únicamente el fallo compuesto:

`TS-SEMA-111 struct type is marked complete after traversal even when fields diagnosed`

El recibo no identifica cuál de sus tres predicados fue falso: existencia del
símbolo, marca `isComplete()` o presencia de diagnósticos.

La lectura estática de `SemanticAnalyzer::analyzeStruct` muestra que registra
el símbolo antes de recorrer campos, sustituye campos inválidos por
`TypeKind::Error` y ejecuta `structType->setComplete(true)` después del
recorrido. Esa evidencia hace plausible el contrato, pero no explica por sí
sola el fallo capturado ni autoriza cambiar producto u oráculo.

## Decisión

No se modifica `TS-SEMA-111`. La siguiente ejecución debe descomponer el
predicado o conservar un recibo con los tres valores observados. Solo esa
evidencia permitirá clasificar el caso como fixture, oráculo o defecto de
producto sin adivinar.

## Validación

No se compiló ni ejecutó por orden del usuario.
