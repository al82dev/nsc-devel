# T08: controles existentes de compatibilidad NSMETA 1.47

Se construyeron incrementalmente `nsc_library_metadata_version_tolerance_tests` y `nsc_stage48_2_library_metadata_tests`. Se ejecutaron con temporales y runtime de desarrollo en `A:/nsc-interop-build`, sin instalar ni desplegar en C:.

El primero pasó 17 comprobaciones. El segundo pasó 430 de 434 y falló al abrir las muestras `tests/fixtures/nsmeta/historical-1.45.nsmeta` y `historical-1.46.nsmeta`, que no están en el repositorio: cada muestra ausente produce dos fallos. Registro: `A:/nsc-interop-build/tanda08-meta-compat-2.log`.

Hay variantes 1.45 recodificadas por el control H8 y versiones 1.45/1.46 generadas por el escritor actual durante la prueba. No se presentaron como muestras independientes históricas ni se copiaron a la carpeta de goldens. Falta disponer de evidencia apropiada para completar este control; no se considera aprobado ni cierra la tanda.
