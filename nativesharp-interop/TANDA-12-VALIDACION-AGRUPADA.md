# Validación agrupada de la tanda 12

Cada corrida ejecuta `build-claude-t08/tools/lote-t12.cmd <etiqueta>`:

- todos los guiones `validar*.py` de `pruebas-tanda-12-*`, `pruebas-tanda-11-*`, `pruebas-tanda-10-*`
  y `pruebas-tanda-09-*`, con cinco procesos;
- `lote-ct2.cmd`: `ninja -k 0`, nueve CTests seleccionadas y `nsc_cpp_c_identity_tests`.

Los guiones de `cpp-tamano-objeto` que fallan con cinco procesos se repiten con uno
(`lote-lista1.cmd`). `k0check.py` resume `ninja -k 0`: errores de compilación y símbolos sin resolver
distintos de los tres conocidos.

## Herencia, despacho y valores con bases (2026-11-15)

El compilador incluye [t12-herencia-y-despacho.md](registro-commits/t12-herencia-y-despacho.md) sobre
el cierre de la tanda 11. Registros en `logs/regresion-t12-h1.log`, `logs/regresion-t11-h1.log`,
`logs/regresion-t10-h1.log`, `logs/regresion-t09-h1.log` y `logs/lote-ct2.log`.

- Tanda 12: 2 guiones, los dos pasan.
- Tanda 11: 5 guiones, todos pasan.
- Tanda 10: 28 guiones, todos pasan.
- Tanda 9: de 1099 guiones pasan 1094 y se omiten 2. Fallan los dos de Clang
  `cpp-variadica-preserve-none` y, en la corrida paralela, `cpp-tamano-objeto/validar-indice-llamada`;
  los tres guiones de `lista-tamano-3.txt` pasan con un proceso (`logs/regresion-t09-h1-tamano.log`).
- `lote-ct2.cmd`: pasan las nueve CTests y `nsc_cpp_c_identity_tests`. `ninja -k 0` no tiene errores de
  compilación y solo falla al enlazar pruebas por los tres símbolos ya conocidos.

La regresión ejecuta cada guion sin `--dll`; las variantes DLL de los fixtures nuevos se ejecutaron
aparte (evidencias en cada registro).

## Operadores miembro (2026-11-15)

El compilador incluye [t12-operadores-miembro.md](registro-commits/t12-operadores-miembro.md) sobre
[t12-herencia-y-despacho.md](registro-commits/t12-herencia-y-despacho.md). Registros en
`logs/regresion-t12-h2.log`, `logs/regresion-t11-h2.log`, `logs/regresion-t10-h2.log`,
`logs/regresion-t09-h2.log` y `logs/lote-ct2.log`.

- Tanda 12: 3 guiones, todos pasan.
- Tanda 11: 5 guiones, todos pasan.
- Tanda 10: 28 guiones, todos pasan.
- Tanda 9: de 1099 guiones pasan 1093 y se omiten 2. Fallan los dos de Clang
  `cpp-variadica-preserve-none` y, en la corrida paralela, `cpp-tamano-objeto/validar-indice-expresion`
  y `validar-indice-llamada`; los tres guiones de `lista-tamano-3.txt` pasan con un proceso
  (`logs/regresion-t09-h2-tamano.log`).
- `lote-ct2.cmd`: pasan las nueve CTests y `nsc_cpp_c_identity_tests`. `ninja -k 0` no tiene errores de
  compilación y solo falla al enlazar pruebas por los tres símbolos ya conocidos.
