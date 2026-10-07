# Tanda universal 16: uniones nativas sin `unsafe`

## Objetivo

Validar las uniones con semántica nativa de almacenamiento compartido, sin forzar
una región `unsafe` ni mantener estado obligatorio de “miembro activo”.

## Cambios

- Los proyectos T03 ya no declaran una edición del lenguaje.
- Leer una representación distinta del mismo almacenamiento es una operación nativa.
- Los joins de control conservan el almacenamiento aunque las ramas escriban miembros
  diferentes.
- El mismo contrato se verifica después de publicar e hidratar NSMETA.
- El productor DLL dejó de envolver el acceso a unión en `unsafe`.

## Archivo

- `tests/unit/NativeSharp3T03CUnionTests.cpp`

Se conservan las comprobaciones de copia, inicialización, layout Win64, encabezado C,
DLL y consumo cross-project. El nombre histórico del CTest no cambia.

## Validación dirigida

Pendiente después del commit: `nsc.ns3.t03.c-unions` en O0. No se ejecuta baseline,
certificación, promoción ni matriz O1/O2/O3.
