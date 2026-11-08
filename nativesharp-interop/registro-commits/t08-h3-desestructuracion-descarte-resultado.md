# T08 H3: resultado de patrón anidado y descarte

El control `d68a7fa68` reprodujo la publicación indebida de la global C `_` en
O0–O3 antes de `c1e98e2f4`. Se construyeron incrementalmente `nsc` y el
inspector de identidad, con build `27c3bd04c66d67ff`.

`pruebas-tanda-08-sombras-plantilla-c/diagnosticar.cmd` terminó con código 0:
el consumidor de la biblioteca precompilada obtuvo los resultados esperados y
el inspector comprobó que no se publicaron las globales C sombreadas ni la
función inline C en los casos cubiertos. Esto no acredita todos los ámbitos ni
sustituye la validación completa de la tanda 8, que permanece abierta.
