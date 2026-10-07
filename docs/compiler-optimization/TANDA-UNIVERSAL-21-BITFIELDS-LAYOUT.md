# Tanda universal 21: layout de bitfields sin edición

## Objetivo

Eliminar la última regla de layout de bitfields condicionada por un marcador de
edición y validar el comportamiento por compilación y ejecución reales.

## Cambios

- `ProjectObjectLayoutModel` deja de buscar `ns3-value` para exigir `repr(...)`.
- Un struct ordinario con bitfields usa su layout nativo normal.
- Se conservan las validaciones reales del tipo base, ancho, constantes y
  restricciones de `repr(transparent)`.
- El proyecto dirigido deja de declarar una edición.

## Archivos

- `src/Frontend/ProjectObjectLayoutModel.cpp`
- `tests/unit/NativeSharp3T13BitFieldIntegerTests.cpp`

## Validación dirigida

Pendiente después del commit: `nsc.ns3.t13.bitfields-integers` en O0. No se
ejecuta baseline, certificación, promoción ni instalación.
