# Tanda universal 19: referencias nativas sin exclusividad implícita

## Objetivo

Alinear la prueba histórica de reborrow con el contrato universal de `ref T`: una
referencia ordinaria es un alias nativo y no activa exclusividad automáticamente.

## Cambios

- Los proyectos dejan de declarar una edición del lenguaje.
- Los casos de alias superpuestos, reborrow, acceso directo, índices e indexers se
  validan como programas aceptados.
- El agregado usado para probar movimientos contiene ownership explícito.
- Se conserva el rechazo de mover el propietario completo mientras vive un alias.
- Se conservan la procedencia de lugares y la ascendencia de reborrow en el dump.

## Archivo

- `tests/unit/NativeSharp3T06ReborrowSplitTests.cpp`

No se reduce cobertura: cambia la expectativa de exclusividad implícita, mientras
lifetime, escape, procedencia, pinning y bloqueo de movimientos siguen cubiertos.

## Validación dirigida

- Construcción dirigida: `nsc_ns3_t06_reborrow_split_tests`, correcta.
- CTest: `nsc.ns3.t06.reborrow-split`, `1/1` correcto en `0.76 s`.
- Linker de productos NativeSharp: `C:\native-sharp\bin\nslink.exe`.
- No se ejecutó baseline, certificación, promoción ni instalación.
