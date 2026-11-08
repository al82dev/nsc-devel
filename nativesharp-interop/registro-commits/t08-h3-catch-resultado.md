# T08 H3: validación de variable de catch

El control `0da40cc5b` mostró que se publicaba la global C homónima en O0–O3.
Tras `c03f10f72`, se construyeron incrementalmente `nsc` y el inspector de
identidad, con build `1965527e723a8dbb`.

`pruebas-tanda-08-sombras-plantilla-c/diagnosticar.cmd` terminó con código 0:
el consumidor de la biblioteca precompilada ejecutó el caso de `catch` y las
regresiones de sombras, y no se publicaron las globales C ni la función inline
comprobadas. El código contempla el filtro de `catch`, pero el control no lo
ejercita. La tanda 8 y su validación completa continúan pendientes.
