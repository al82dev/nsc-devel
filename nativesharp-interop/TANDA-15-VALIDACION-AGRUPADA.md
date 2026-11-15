# Validación agrupada de la tanda 15

Cada corrida ejecuta `build-claude-t08/tools/lote-t15.cmd <etiqueta>`:

- todos los guiones `validar*.py` de `pruebas-tanda-15-*` a `pruebas-tanda-09-*`, con cinco procesos;
- `lote-ct2.cmd`: `ninja -k 0`, nueve CTests seleccionadas y `nsc_cpp_c_identity_tests`.

Los guiones de `cpp-tamano-objeto` que fallan con cinco procesos se repiten con uno
(`lote-lista1.cmd`). `k0check.py` resume `ninja -k 0`: errores de compilación y símbolos sin resolver
distintos de los tres conocidos.

## Runtime de C++, excepciones, callbacks y recursos (2026-11-15)

El compilador incluye los cambios de la tanda hasta `12122afa4` (registros
[runtime](registro-commits/t15-runtime-cpp.md), [excepciones](registro-commits/t15-excepciones.md),
[callbacks](registro-commits/t15-callbacks.md) y [recursos](registro-commits/t15-recursos.md)) sobre el cierre
de la tanda 14. Registros en `logs/regresion-t15-g1.log` a `logs/regresion-t09-g1.log` y `logs/lote-ct2.log`.

- Tanda 15: 3 guiones, todos pasan.
- Tanda 14: 4 guiones, todos pasan.
- Tanda 13: 6 guiones, todos pasan.
- Tanda 12: 3 guiones, todos pasan.
- Tanda 11: 5 guiones, todos pasan.
- Tanda 10: 28 guiones, todos pasan.
- Tanda 9: de 1099 guiones pasan 1093 y se omiten 2. Fallan los dos de Clang
  `cpp-variadica-preserve-none` y, en la corrida paralela, `cpp-tamano-objeto/validar-indice-expresion` y
  `validar-indice-llamada`; los tres guiones de `lista-tamano-3.txt` pasan con un proceso
  (`logs/regresion-t09-g1-tamano-t15.log`).
- `lote-ct2.cmd`: pasan las nueve CTests y `nsc_cpp_c_identity_tests`. `ninja -k 0` no tiene errores de
  compilación y solo falla al enlazar pruebas por los tres símbolos ya conocidos.

Con este compilador, los objetos inline de C++ pasan por el pipeline de Clang y llevan las definiciones que
alcanzan, y las tandas 10 a 14 siguen pasando con él.

La regresión ejecuta cada guion sin `--dll`; las variantes DLL de los fixtures nuevos se ejecutaron aparte
(evidencias en cada registro).

## Cierre de la tanda 15 (2026-11-15)

Cada casilla T15 tiene su evidencia frente a Clang 23.1.0 en estática y DLL, O0–O3, y los pendientes que
quedan fuera están en [t15-cierre.md](registro-commits/t15-cierre.md), cada uno con la tanda o el ítem que
lo continúa. No se modificó Clang ni NSLink.
