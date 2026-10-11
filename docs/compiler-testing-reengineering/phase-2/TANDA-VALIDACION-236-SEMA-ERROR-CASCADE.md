# Tanda de validación 236 — sentinel Error sin cascadas derivadas

Fecha: 2026-10-10

## Estado

IMPLEMENTADA. VALIDACIÓN DE PRODUCTO PENDIENTE.

## Causa confirmada

Tres consumidores volvían a diagnosticar una expresión cuyo productor ya
había entregado `TypeKind::Error`:

- el cast agregaba `InvalidConversion` tras `UnknownMember`;
- el acceso a miembro normalizado desde `->` agregaba un segundo
  `InvalidMemberAccess` tras el dereference causal;
- la indexación agregaba `InvalidIndex` tras un error en base o índice.

## Corrección

`analyzeCast`, `analyzeMember` y `analyzeIndex` propagan el sentinel mediante
`makeError` sin inventar un diagnóstico consumidor. Las validaciones propias
se conservan cuando sus operandos sí tienen tipos analizables.

## Alcance

La causa compartida corresponde a `TS-SEMA-216`, `TS-SEMA-219` y
`TS-SEMA-222`.

## Validación

No se compiló ni ejecutó por orden del usuario. La validación queda delegada a
su corrida posterior.
