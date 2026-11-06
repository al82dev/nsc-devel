# Observación de catálogos C precompilados

La observación preliminar del consumidor con encabezado Clang y biblioteca NS
fallaba con `unknown type name 'CRecord'` en el módulo precompilado. Ahora
reconstruye sus declaraciones C en las dos búsquedas de nombres y distingue
sus índices de los grupos observados en vivo. Las identidades coincidentes se
rechazan hasta contar con una conciliación comprobada; no se equiparan por nombre.

La construcción incremental de `nsc` pasó. En la prueba agrupada del catálogo,
los casos previos pasaron en O0–O3; el escenario mixto O0 ya supera el error de
tipo desconocido, pero se detiene en `C/C++ type alias conflicts with a native
type 'CRecord'`. La batería completa terminó con código 1. El caso mixto no
demuestra aún que dos importaciones del mismo encabezado sean intercambiables.

La búsqueda de alias considera tanto declaraciones visibles como alias Clang;
la biblioteca productora y el consumidor asignan identidades de módulo distintas
al mismo nombre. Se requiere conciliar procedencia y contratos antes de permitir
conversiones o descartar una declaración. El USR o el nombre por sí solos no
son prueba suficiente de equivalencia entre encabezados y configuraciones.
