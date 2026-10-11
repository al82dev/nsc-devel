# Tanda de validación 225 — NCD Dominance a escala

Fecha: 2026-10-10

## Estado

IMPLEMENTADA. VALIDACIÓN CONJUNTA PENDIENTE.

## Causa y corrección

`TF-DOM-080` materializaba el oráculo completo de dominadores y recorría la
matriz cartesiana de pares para un CFG lineal de 2048 nodos. La prueba NIGHTLY
de escala conserva el contrato observable mediante tres consultas
representativas: raíz con cola, ancestro intermedio con cola e identidad de la
cola. Las variantes pequeñas conservan la matriz exhaustiva independiente.

## Validación

No se compiló ni ejecutó por orden del usuario. La validación queda delegada a
la corrida posterior del usuario.
