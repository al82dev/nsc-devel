# Validación agrupada de la tanda 11

Cada corrida ejecuta `build-claude-t08/tools/lote-t11.cmd <etiqueta>`:

- todos los guiones `validar*.py` de `pruebas-tanda-11-*`, `pruebas-tanda-10-*` y
  `pruebas-tanda-09-*`, con cinco procesos;
- `lote-ct2.cmd`: `ninja -k 0`, nueve CTests seleccionadas y `nsc_cpp_c_identity_tests`.

Los guiones de `cpp-tamano-objeto` que fallan con cinco procesos se repiten con uno
(`lote-lista1.cmd`).

## Destructores noexcept implícitos y métodos de instancia (2026-11-15)

El compilador incluye [t11-destructores-noexcept.md](registro-commits/t11-destructores-noexcept.md) y
[t11-metodos-instancia.md](registro-commits/t11-metodos-instancia.md) sobre `9fa734dd4`. La corrida se
hizo con los dos cambios juntos, que se registran en dos commits. Registros en `logs/regresion-t11-m3/`,
`logs/regresion-t10-m3/` y `logs/regresion-t09-m3/`.

- Tanda 11: 2 guiones, los dos pasan.
- Tanda 10: 28 guiones, todos pasan.
- Tanda 9: de 1099 guiones pasan 1094 y se omiten 2. Fallan los dos de Clang
  `cpp-variadica-preserve-none` y `cpp-tamano-objeto/validar-indice-expresion` en la corrida paralela;
  los tres guiones de `lista-tamano-3.txt` pasan con un proceso (`logs/regresion-t09-m3-tamano.log`).
- `lote-ct2.cmd`: pasan las nueve CTests y `nsc_cpp_c_identity_tests`. `ninja -k 0` solo falla por
  los tres símbolos ya conocidos.

La regresión ejecuta cada guion sin `--dll`; las variantes DLL de los fixtures nuevos se ejecutaron
aparte (evidencias en cada registro).
