# Tanda de validación 241 — precondiciones de layout y dump canónico

Fecha: 2026-10-10

## Estado

IMPLEMENTADA. VALIDACIÓN PENDIENTE.

## Causa confirmada

Cinco oráculos construían tipos nominales incompletos y después exigían
layout, `offsetof` o recorrido ABI como si hubieran terminado su declaración.
Además, `TS-SEMA-237` esperaba layout para un array de longitud cero, aunque
el contrato de tipos fijos exige una longitud positiva, y `TS-SEMA-311`
esperaba nombres de enum C++ en vez de los nombres canónicos de
`ConversionKind::ToString`.

## Corrección

- `TS-SEMA-237` trata longitud cero y overflow como layout ausente.
- `238`, `240` y `266` marcan completos los structs que el fixture termina de
  construir antes de consultar layout, offsetof o compatibilidad ABI.
- `311` compara `integral-promotion`, `implicit-numeric` y
  `explicit-numeric`, que son los textos públicos del dumper.

## Alcance

Solo se corrigen precondiciones y valores esperados de pruebas. El layout, la
ABI y el dumper del compilador no cambian.

## Validación

No se compiló ni ejecutó por orden del usuario. La validación queda delegada a
su corrida posterior.
