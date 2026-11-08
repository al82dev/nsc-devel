# T08 H3: validación de referencias locales en bloques

Con `nsc` de desarrollo build `dcd50cbba291498b`, la construcción incremental
de `nsc` y `nsc_cpp_c_identity_tests` terminó con código 0.

- `diagnosticar.cmd`: código 0 en O0–O3; la global y la función inline C
  homónimas de las variables locales no se publican ni emiten indebidamente.
- `validar-ambito.cmd`: código 0 en O0–O3; la llamada C anterior y posterior
  a un bloque con variable homónima sigue funcionando (resultado 179).
- `pruebas-tanda-08-identidad-c/validar.cmd`: código 0; incluye pruebas de
  identidad, consumo precompilado, plantillas, enumeradores y catálogos.

El filtrado cubre el caso demostrado de declaraciones locales anteriores en
bloques. Parámetros, declaraciones en condiciones o bucles y otras formas
requieren análisis y controles propios. Esto no cierra T08-002 ni reemplaza la
validación completa de la tanda 8.
