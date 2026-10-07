# Tanda universal 18: movimientos parciales con ownership explícito

## Objetivo

Conservar la cobertura completa de movimientos parciales sin depender de una edición
del lenguaje que convierta implícitamente todos los structs en move-only.

## Cambios

- Los proyectos dirigidos dejan de declarar una edición del lenguaje.
- Los agregados sujetos a movimientos parciales contienen recursos `owned` reales.
- El fixture de destructor conserva su destructor observable y añade ownership explícito.
- Se mantienen las verificaciones de doble movimiento, movimiento completo posterior,
  joins de ramas, back-edges de ciclos y destructores que observan el valor completo.

## Archivo

- `tests/unit/NativeSharp3T06PartialMoveTests.cpp`

El nombre histórico del CTest se conserva. La prueba sigue verificando el mismo
contrato de seguridad, pero ahora la capacidad move-only nace de la representación.

## Validación dirigida

Pendiente después del commit: `nsc.ns3.t06.partial-moves` en O0. No se ejecuta
baseline, certificación, promoción ni instalación.
