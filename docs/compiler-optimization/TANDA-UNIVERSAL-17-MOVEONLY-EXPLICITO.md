# Tanda universal 17: move-only explícito por ownership

## Objetivo

Separar la semántica de valor ordinaria de la semántica move-only mediante ownership
explícito en la representación, sin depender de una edición del lenguaje.

## Cambios

- Los fixtures que prueban restricciones de movimiento contienen un campo `owned`.
- Los structs ordinarios conservan copia estructural implícita.
- `copy struct` sigue publicando una capacidad explícita y verificable en NSMETA.
- Se mantienen los rechazos por copia, uso después de move, joins, parámetros,
  constructores, retornos, arrays y `where T : copy` para tipos move-only explícitos.
- Los proyectos ya no emiten `<Language>2</Language>` ni `<Language>3</Language>`.

## Archivo

- `tests/unit/NativeSharp3T06MoveOnlyTests.cpp`

El nombre histórico del CTest no cambia. La cobertura deja de depender de un default
oculto y pasa a probar directamente las dos representaciones del lenguaje.

## Validación dirigida

- Construcción dirigida: `nsc_ns3_t06_move_only_tests`, correcta.
- CTest: `nsc.ns3.t06.move-only-defaults`, `1/1` correcto en `0.81 s`.
- Linker de productos NativeSharp: `C:\native-sharp\bin\nslink.exe`.
- No se ejecutó baseline, certificación, promoción ni matriz O1/O2/O3.
