# Tanda universal 17: `moveonly` explícito

## Objetivo

Separar la semántica de valor ordinaria de la semántica move-only mediante sintaxis
explícita, sin depender de una edición del lenguaje.

## Cambios

- Los fixtures que prueban restricciones de movimiento declaran `moveonly struct`.
- Los structs ordinarios conservan copia estructural implícita.
- `copy struct` sigue publicando una capacidad explícita y verificable en NSMETA.
- Se mantienen los rechazos por copia, uso después de move, joins, parámetros,
  constructores, retornos, arrays y `where T : copy` para tipos move-only explícitos.
- Los proyectos ya no emiten `<Language>2</Language>` ni `<Language>3</Language>`.

## Archivo

- `tests/unit/NativeSharp3T06MoveOnlyTests.cpp`

El nombre histórico del CTest no cambia. La cobertura deja de depender de un default
oculto y pasa a probar directamente las dos capacidades del lenguaje.

## Validación dirigida

Pendiente después del commit: `nsc.ns3.t06.move-only-defaults` en O0. No se ejecuta
baseline, certificación, promoción ni matriz O1/O2/O3.
