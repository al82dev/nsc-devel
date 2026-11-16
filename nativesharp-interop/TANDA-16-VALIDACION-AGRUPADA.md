# Validación agrupada de la tanda 16

Cada corrida ejecuta `build-claude-t08/tools/lote-t16.cmd <etiqueta>`:

- todos los guiones `validar*.py` de `pruebas-tanda-16-*` a `pruebas-tanda-09-*`, con cinco procesos;
- `lote-ct2.cmd`: `ninja -k 0`, nueve CTests seleccionadas y `nsc_cpp_c_identity_tests`.

Los guiones de `cpp-tamano-objeto` que fallan con cinco procesos se repiten con uno
(`lote-lista1.cmd`). `k0check.py` resume `ninja -k 0`: errores de compilación y símbolos sin resolver
distintos de los tres conocidos.

## Arranque, TLS, enlace y atómicos (2026-11-15)

El compilador incluye los cambios de la tanda hasta `47df534c9` (el único cambio de producción es
[inicializadores](registro-commits/t16-inicializadores.md), `bb4c421a1`) sobre el cierre de la tanda 15.
Registros en `logs/regresion-t16-t16b.log` a `logs/regresion-t10-t16b.log` y `logs/lote-ct2.log`.

- Tanda 16: 4 guiones, todos pasan.
- Tanda 15: 3 guiones, todos pasan.
- Tanda 14: 4 guiones, todos pasan.
- Tanda 13: 6 guiones, todos pasan.
- Tanda 12: 3 guiones, todos pasan.
- Tanda 11: 5 guiones, todos pasan.
- Tanda 10: 28 guiones, todos pasan.
- Tanda 9: de 1099 guiones pasan 1094 y se omiten 2. Fallan los dos de Clang
  `cpp-variadica-preserve-none` y, en la corrida paralela, `cpp-tamano-objeto/validar-indice-expresion`; los
  tres guiones de `lista-tamano-3.txt` pasan con un proceso (`logs/regresion-t09-t16b-tamano.log`). La
  repetición serial usó la misma etiqueta y reemplazó el resumen de `logs/regresion-t09-t16b.log`; los
  totales de la corrida paralela son los que se citan aquí.
- `lote-ct2.cmd`: pasan las nueve CTests y `nsc_cpp_c_identity_tests`. `ninja -k 0` no tiene errores de
  compilación y solo falla al enlazar pruebas por los tres símbolos ya conocidos.

Con este compilador, el rechazo de inicializadores que no se ejecutarían no afecta a las referencias al CRT
de las tandas 8 y 10 ni a ningún guion anterior.

La regresión ejecuta cada guion sin `--dll`; las variantes DLL de los fixtures nuevos se ejecutaron aparte
con el mismo compilador: arranque `A:/temp/nsc-t16-arranque-ccahldl2`, TLS `A:/temp/nsc-t16-tls-ftrt9hcs`,
atómicos `A:/temp/nsc-t16-atomicos-vn7apta0`, enlace `A:/temp/nsc-t16-enlace-c8gqsa_c`.

## Cierre de la tanda 16 (2026-11-15)

Cada casilla T16 tiene su evidencia frente a Clang 23.1.0 en estática y DLL, O0–O3, o su política definida
sobre el código y los fixtures, y los pendientes que quedan fuera están en
[t16-cierre.md](registro-commits/t16-cierre.md), cada uno con la tanda o el ítem que lo continúa. No se
modificó Clang ni NSLink.
