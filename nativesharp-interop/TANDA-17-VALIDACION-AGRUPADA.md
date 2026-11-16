# Validación agrupada de la tanda 17

Cada corrida ejecuta `build-claude-t08/tools/lote-t17.cmd <etiqueta>`:

- todos los guiones `validar*.py` de `pruebas-tanda-17-*` a `pruebas-tanda-09-*`, con cinco procesos;
- `lote-ct2.cmd`: `ninja -k 0`, nueve CTests seleccionadas y `nsc_cpp_c_identity_tests`.

Los guiones que fallan se repiten con `lote-lista5.cmd` (cinco procesos) y los de `cpp-tamano-objeto` con
`lote-lista1.cmd` (uno), cada uno con su propia etiqueta. `k0check.py` resume `ninja -k 0`.

## Diagnósticos, depuración y caché (2026-11-15)

El compilador incluye los cambios de la tanda hasta `ae9412e70` (registros
[ubicaciones](registro-commits/t17-ubicaciones.md), [sobrecargas](registro-commits/t17-sobrecargas.md),
[símbolos](registro-commits/t17-simbolos.md), [depuración](registro-commits/t17-depuracion.md) y
[caché](registro-commits/t17-cache.md)) sobre el cierre de la tanda 16. Registros en
`logs/regresion-t17-t17r.log` a `logs/regresion-t09-t17r.log` y `logs/lote-ct2.log`.

- Tanda 17: 3 guiones, todos pasan.
- Tanda 16: 4 guiones, todos pasan.
- Tanda 15: 3 guiones, todos pasan.
- Tanda 14: 4 guiones, todos pasan.
- Tanda 13: 6 guiones, todos pasan.
- Tanda 12: 3 guiones, todos pasan.
- Tanda 11: 5 guiones, todos pasan.
- Tanda 10: 28 guiones, todos pasan.
- Tanda 9, primera corrida: de 1099 guiones pasan 884, se omiten 2 y fallan 213. 210 de las fallas son la
  aserción `compilation-bundle-cache: disabled` que el cambio 5 hizo obsoleta en 150 guiones C++; se
  convirtieron en una comparación positiva ([registro](registro-commits/t17-cache-pruebas-t09.md),
  `f51ce3747`) y, repetidas, pasan las 210 (`logs/regresion-t09-t17r-repeticion.log`). Quedan los dos de
  Clang `cpp-variadica-preserve-none` y, en la corrida paralela, `cpp-tamano-objeto/validar-indice-expresion`;
  los tres de `lista-tamano-3.txt` pasan con un proceso (`logs/regresion-t09-t17r-tamano.log`). Es la línea
  base de las tandas anteriores: 1094 de 1099.
- `lote-ct2.cmd`: pasan las nueve CTests y `nsc_cpp_c_identity_tests`. `ninja -k 0` no tiene errores de
  compilación y solo falla al enlazar pruebas por los tres símbolos ya conocidos.

La regresión ejecuta cada guion sin `--dll`; las variantes DLL de los fixtures nuevos se ejecutaron aparte con
el mismo compilador: diagnósticos `A:/temp/nsc-t17-diagnosticos-f1n2ug5g`, depuración
`A:/temp/nsc-t17-depuracion-yol891_j`, caché `A:/temp/nsc-t17-cache-mw01xmqd`.

## Cierre de la tanda 17 (2026-11-15)

Cada casilla T17 tiene su evidencia frente a Clang 23.1.0 en estática y DLL, y los pendientes que quedan
fuera están en [t17-cierre.md](registro-commits/t17-cierre.md), cada uno con la tanda o el ítem que lo
continúa. No se modificó Clang ni NSLink.
