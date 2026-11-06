# Observación de catálogos C precompilados

La observación preliminar del consumidor con encabezado Clang y biblioteca NS
fallaba con `unknown type name 'CRecord'` en el módulo precompilado. Ahora
reconstruye sus declaraciones C en las dos búsquedas de nombres y distingue
sus índices de los grupos observados en vivo. Las identidades coincidentes se
rechazan hasta contar con una conciliación comprobada; no se equiparan por nombre.

El cambio está pendiente de construcción incremental y validación dirigida. No
demuestra aún que dos importaciones del mismo encabezado sean intercambiables.
