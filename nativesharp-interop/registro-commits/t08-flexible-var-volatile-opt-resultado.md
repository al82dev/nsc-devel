# T08-010/015: comparación del alias volatile en O0–O3

`diagnosticar-var-volatile.cmd`, commit `6235129a0`, terminó con código 1 con
NSC build `5c1e6c7fa15d8a3e` y Clang 23.1.0. En las cuatro optimizaciones:

- Microsoft: Clang emite lectura acquire y escritura release atómicas y
  volátiles; NSC emite accesos volátiles sin orden. Ambos usan alineación 4.
- ISO: ambos emiten accesos volátiles sin orden, con alineación 4.

Los IR por modo y optimización están en
`A:/nsc-interop-build/tanda08-flexible-var-volatile/`. Se inspeccionaron las
dos funciones aisladas mediante el comparador; no se probó concurrencia ni se
ha corregido la política de modo para la unidad consumidora. La tanda sigue
abierta.
