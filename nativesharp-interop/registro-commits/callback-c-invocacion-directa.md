# Llamada directa a callback C almacenado

El fixture `DirectStored.ns` comprueba una llamada directa a un callback
escalar `int(int)` almacenado en un campo C y a otro en un global C. La prueba
usa los mismos valores y biblioteca que `Stored.ns`, que primero copia los
punteros a variables locales.

El compilador existente generó el IR O0 con código 0. En
`A:/nsc-interop-build/tanda08-callback/direct-stored.ll` se observa la carga
del campo y su llamada indirecta, así como la carga del global y su llamada
indirecta, ambas con argumento 39. Esto no demuestra la ejecución: se agregó el
caso a `validar.cmd` para compilarlo, enlazarlo y ejecutarlo en O0–O3 junto con
el resto del grupo. El fixture de callback con puntero a record incluye ahora
también las llamadas directas, pendientes de construcción y validación.

Actualización: con NSC build `3f82235fd35e7b9a`, `validar.cmd` terminó con
código 0 y ejecutó la llamada directa escalar en O0–O3. También ejecutó el
caso directo de firma con puntero a record. Son casos dirigidos; no cierran
T08-017.
