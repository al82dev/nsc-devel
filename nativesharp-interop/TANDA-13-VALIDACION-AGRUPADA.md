# Validación agrupada de la tanda 13

Cada corrida ejecuta `build-claude-t08/tools/lote-t13.cmd <etiqueta>`:

- todos los guiones `validar*.py` de `pruebas-tanda-13-*`, `pruebas-tanda-12-*`, `pruebas-tanda-11-*`,
  `pruebas-tanda-10-*` y `pruebas-tanda-09-*`, con cinco procesos;
- `lote-ct2.cmd`: `ninja -k 0`, nueve CTests seleccionadas y `nsc_cpp_c_identity_tests`.

Los guiones de `cpp-tamano-objeto` que fallan con cinco procesos se repiten con uno
(`lote-lista1.cmd`). `k0check.py` resume `ninja -k 0`: errores de compilación y símbolos sin resolver
distintos de los tres conocidos.

## Plantillas, constexpr, structs NativeSharp y COMDAT (2026-11-15)

El compilador incluye todos los cambios de la tanda hasta `7729603f2` (registros
[especializaciones](registro-commits/t13-especializaciones.md),
[plantillas](registro-commits/t13-plantillas-ns.md), [deducción](registro-commits/t13-deduccion.md),
[constexpr](registro-commits/t13-constexpr.md), [structs](registro-commits/t13-structs-ns.md) y
[COMDAT](registro-commits/t13-comdat.md)) sobre el cierre de la tanda 12. Registros en
`logs/regresion-t13-g1.log`, `logs/regresion-t12-g1.log`, `logs/regresion-t11-g1.log`,
`logs/regresion-t10-g1.log`, `logs/regresion-t09-g1.log` y `logs/lote-ct2.log`.

- Tanda 13: 6 guiones, todos pasan.
- Tanda 12: 3 guiones, todos pasan.
- Tanda 11: 5 guiones, todos pasan.
- Tanda 10: 28 guiones, todos pasan.
- Tanda 9: de 1099 guiones pasan 1093 y se omiten 2. Fallan los dos de Clang
  `cpp-variadica-preserve-none` y, en la corrida paralela, `cpp-tamano-objeto/validar-indice-expresion`
  y `validar-indice-llamada`; los tres guiones de `lista-tamano-3.txt` pasan con un proceso
  (`logs/regresion-t09-g1-tamano.log`).
- `lote-ct2.cmd`: pasan las nueve CTests y `nsc_cpp_c_identity_tests`. `ninja -k 0` no tiene errores de
  compilación y solo falla al enlazar pruebas por los tres símbolos ya conocidos.

La regresión ejecuta cada guion sin `--dll`; las variantes DLL de los fixtures nuevos se ejecutaron
aparte (evidencias en cada registro). La sonda de la auditoría pasa sus 12 casos.

## Cierre de la tanda 13 (2026-11-15)

Esta corrida contiene todos los cambios de la tanda. Cada casilla T13 tiene su evidencia frente a Clang
23.1.0 en estática y DLL, O0–O3, y los pendientes que quedan fuera están en
[t13-cierre.md](registro-commits/t13-cierre.md), cada uno con la tanda o el ítem que lo continúa. No se
modificó Clang ni NSLink.
