# Tanda de validación 240 — diagnósticos semánticos exactos

Fecha: 2026-10-10

## Estado

IMPLEMENTADA. VALIDACIÓN PENDIENTE.

## Causa confirmada

Los oráculos de `TS-SEMA-173`, `204` y `220` solo declaraban el primer
diagnóstico de escenarios que, por contrato, continúan analizando una
inicialización, un argumento excedente o una asignación. El ejecutor compara
la secuencia completa y por ello rechazaba la salida causal real.

## Corrección

- `173` conserva `InvalidAddressOf` y la conversión inválida derivada del
  inicializador de `p`.
- `204` conserva `InvalidArgumentCount` y analiza el argumento excedente,
  produciendo `UnknownIdentifier`.
- `220` declara primero que el local `const` carece de inicializador constante
  y después que su miembro no es un destino modificable.

## Alcance

Solo se completa el oráculo ordenado de diagnósticos ya emitidos. No se
modifica el compilador ni se relaja ningún contrato.

## Validación

No se compiló ni ejecutó por orden del usuario. La validación queda delegada a
su corrida posterior.
