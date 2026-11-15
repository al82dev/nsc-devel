# Validación agrupada de la tanda 10

Cada corrida ejecuta `build-claude-t08/tools/lote-t10.cmd <etiqueta>`:

- todos los guiones `validar*.py` de `pruebas-tanda-10-*` y de `pruebas-tanda-09-*`, con cinco
  procesos;
- `lote-ct2.cmd`: `ninja -k 0`, nueve CTests seleccionadas y `nsc_cpp_c_identity_tests`.

Los guiones de `cpp-tamano-objeto` que fallan con cinco procesos se repiten con uno
(`lote-lista1.cmd`).

## Referencias lvalue y argumentos predeterminados (2026-11-15)

El compilador incluye [t10-referencias-lvalue.md](registro-commits/t10-referencias-lvalue.md)
(`142dae71e`) y [t10-argumentos-predeterminados.md](registro-commits/t10-argumentos-predeterminados.md).
Registros en `logs/regresion-t10-d2/` y `logs/regresion-t09-d2/`.

- Tanda 10: 19 guiones, todos pasan.
- Tanda 9: de 1099 guiones pasan 1092 y se omiten 2. Fallan:
  - los dos de Clang `cpp-variadica-preserve-none`, por la causa documentada en la tanda 9;
  - tres de `cpp-tamano-objeto` en la corrida paralela (`validar-indice-expresion`,
    `validar-indice-llamada` y `validar-puntero-desplazado`), que pasan con un proceso
    (`logs/regresion-t09-d2-tamano.log`).
- `lote-ct2.cmd`: pasan las nueve CTests y `nsc_cpp_c_identity_tests`. `ninja -k 0` solo falla por
  los tres símbolos ya conocidos.

## Definiciones inline emitidas por Clang (2026-11-15)

El compilador incluye además [t10-inline-emitidas-por-clang.md](registro-commits/t10-inline-emitidas-por-clang.md)
sobre `e999e5217`. Registros en `logs/regresion-t10-i3/` y `logs/regresion-t09-i3/`.

- Tanda 10: 20 guiones, todos pasan.
- Tanda 9: de 1099 guiones pasan 1093 y se omiten 2. Fallan los dos de Clang
  `cpp-variadica-preserve-none` y dos de `cpp-tamano-objeto` en la corrida paralela, que pasan con
  un proceso (`logs/regresion-t09-i3-tamano.log`).
- `lote-ct2.cmd`: pasan las nueve CTests y `nsc_cpp_c_identity_tests`. `ninja -k 0` solo falla por
  los tres símbolos ya conocidos.

## Nombres de namespace: inline namespaces, alias y using (2026-11-15)

El compilador incluye además [t10-nombres-namespace.md](registro-commits/t10-nombres-namespace.md)
sobre `0c3dd5796`. Registros en `logs/regresion-t10-n3/` y `logs/regresion-t09-n3/`.

- Tanda 10: 21 guiones, todos pasan.
- Tanda 9: de 1099 guiones pasan 1095 y se omiten 2. Solo fallan los dos de Clang
  `cpp-variadica-preserve-none`; esta vez los de `cpp-tamano-objeto` pasan también en la corrida
  paralela.
- `lote-ct2.cmd`: pasan las nueve CTests y `nsc_cpp_c_identity_tests`. `ninja -k 0` solo falla por
  los tres símbolos ya conocidos.

## Records y enums anidados en records (2026-11-15)

El compilador incluye además [t10-tipos-anidados.md](registro-commits/t10-tipos-anidados.md) sobre
`051780fa4`. Registros en `logs/regresion-t10-e6/` y `logs/regresion-t09-e6/`.

- Tanda 10: 22 guiones, todos pasan.
- Tanda 9: de 1099 guiones pasan 1094 y se omiten 2. Fallan los dos de Clang
  `cpp-variadica-preserve-none` y `cpp-tamano-objeto/validar-puntero-desplazado` en la corrida
  paralela; los tres guiones de `lista-tamano-3.txt` pasan con un proceso
  (`logs/regresion-t09-e6-tamano.log`).
- `lote-ct2.cmd`: pasan las nueve CTests y `nsc_cpp_c_identity_tests`. `ninja -k 0` solo falla por
  los tres símbolos ya conocidos.

## Globales y campos estáticos de record, enum, arreglo y thread_local (2026-11-15)

El compilador incluye además [t10-globales-y-estaticos.md](registro-commits/t10-globales-y-estaticos.md)
sobre `28defb1be`. Registros en `logs/regresion-t10-g3/` y `logs/regresion-t09-g3/`.

- Tanda 10: 23 guiones, todos pasan.
- Tanda 9: de 1099 guiones pasan 1094 y se omiten 2. Fallan los dos de Clang
  `cpp-variadica-preserve-none` y `cpp-tamano-objeto/validar-indice-expresion` en la corrida paralela;
  los tres guiones de `lista-tamano-3.txt` pasan con un proceso (`logs/regresion-t09-g3-tamano.log`).
- `lote-ct2.cmd`: pasan las nueve CTests y `nsc_cpp_c_identity_tests`. `ninja -k 0` solo falla por
  los tres símbolos ya conocidos.
