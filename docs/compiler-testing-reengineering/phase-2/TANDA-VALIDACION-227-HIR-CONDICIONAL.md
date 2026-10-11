# Tanda de validación 227 — productor HIR condicional

Fecha: 2026-10-10

## Estado

IMPLEMENTADA. VALIDACIÓN DE PRODUCTO PENDIENTE.

## Causa confirmada

La tubería modular no tenía un contrato completo para
`ConditionalExpression`: Sema no registraba tipo ni conversiones y
`HirBuilder::lowerExpression` no podía producir una representación HIR. El
caso `TF-HIR-120` llegaba por tanto sin un producto verificable.

## Corrección

- Sema exige condición `bool`, elige un tipo común para las ramas, registra
  conversiones por operando y conserva constante cuando la condición y la rama
  seleccionada son constantes.
- HIR incorpora el nodo `Conditional` con condición, rama verdadera y rama
  falsa en aristas distintas.
- El verifier HIR exige que ambas ramas tengan exactamente el tipo resultante.
- `TF-HIR-120` observa explícitamente la identidad y las tres aristas del nodo,
  en lugar de inferir soporte por un `Binary` interno del fixture.

## Dependencias derivadas

`TF-MIR-152`, `TF-MIR-183`, `TF-MIR-195`, `TF-MIR-198`, `TF-MIR-200` y
`TF-MIR-270` continúan clasificados como `BLOQUEADO_POR TF-HIR-120`. Por orden
de separación causal no se modificó MIR antes de validar el productor HIR.

## Validación

No se compiló ni ejecutó por orden del usuario. La validación queda delegada a
su corrida posterior; no se afirma que el consumidor MIR acepte aún el nuevo
nodo.
