# T08-002-08: búsqueda de muestra independiente NSMETA 1.46

Se buscó en las carpetas `A:/nsc-*` existentes, sin modificarlas, archivos
`.nsmeta` cuyo encabezado binario fuera `NSMETA1`, mayor 1 y minor 46. La
utilidad de búsqueda quedó en `A:/nsc-interop-build/buscar_nsmeta_146.py`.

Se encontraron dos archivos `historical-1.46.nsmeta`, ambos bajo el directorio
temporal `Integrity` de `nativesharp_stage48_2_library_metadata_tests`:

- `tanda08-nsmeta-temp`, productor `20261108T075734Z.220d44fcec18415c`.
- `tanda08-meta-compat/temp`, productor `20261108T070348Z.5ac74f49e40d7f2f`.

Estos archivos fueron producidos por el control posterior, como ya advierte
`t08-nsmeta-147-compatibilidad.md`; no se copiaron como goldens independientes.
Los productores de alias, función y global estática revisados con el verificador
actual son NSMETA 1.47. No se encontró un archivo con marca de compilación entre
la introducción de 1.46 y el commit que introdujo 1.47 en la búsqueda realizada.

Siguiente paso: localizar un compilador histórico ya construido o producir el
fixture desde el escritor histórico de 1.46 de forma independiente, agrupando
esa construcción con la validación del bloque. La casilla y T08-002 siguen
abiertos; no se ha repetido la suite completa de compatibilidad.
