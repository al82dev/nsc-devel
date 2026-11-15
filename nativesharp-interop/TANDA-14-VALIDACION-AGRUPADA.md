# Validación agrupada de la tanda 14

Cada corrida ejecuta `build-claude-t08/tools/lote-t14.cmd <etiqueta>`:

- todos los guiones `validar*.py` de `pruebas-tanda-14-*` a `pruebas-tanda-09-*`, con cinco procesos;
- `lote-ct2.cmd`: `ninja -k 0`, nueve CTests seleccionadas y `nsc_cpp_c_identity_tests`.

Los guiones de `cpp-tamano-objeto` que fallan con cinco procesos se repiten con uno
(`lote-lista1.cmd`). `k0check.py` resume `ninja -k 0`: errores de compilación y símbolos sin resolver
distintos de los tres conocidos.

## Macros, configuración, registros y declaraciones especiales (2026-11-15)

El compilador incluye los cambios de la tanda hasta `3d1c1431c` (registros
[macros](registro-commits/t14-macros.md), [configuración](registro-commits/t14-configuracion.md),
[registros](registro-commits/t14-registros.md) y [especiales](registro-commits/t14-especiales.md)) sobre el
cierre de la tanda 13. Registros en `logs/regresion-t14-g1.log` a `logs/regresion-t09-g1.log` y
`logs/lote-ct2.log`.

- Tanda 14: 4 guiones, todos pasan.
- Tanda 13: 6 guiones, todos pasan.
- Tanda 12: 3 guiones, todos pasan.
- Tanda 11: 5 guiones, todos pasan.
- Tanda 10: de 28 guiones pasan 27; falla `pruebas-tanda-10-referencias/validar.py`.
- Tanda 9: de 1099 guiones pasan 1093 y se omiten 2. Fallan los dos de Clang
  `cpp-variadica-preserve-none`, en la corrida paralela `cpp-tamano-objeto/validar-indice-expresion` y
  `pruebas-tanda-09-sysv-cpp-record-directo/validar-rechazos.py`.
- `lote-ct2.cmd`: pasan las nueve CTests y `nsc_cpp_c_identity_tests`. `ninja -k 0` no tiene errores de
  compilación y solo falla al enlazar pruebas por los tres símbolos ya conocidos.

Los dos fallos nuevos son negativos que siguen rechazando, pero buscan el prefijo
`unknown identifier '<namespace>'`, que el diagnóstico calificado de T14-007 había sustituido por el motivo.
El cambio `40b94aa77` conserva el prefijo delante del motivo. Con ese compilador:

- `pruebas-tanda-10-referencias/validar.py` pasa (`A:/temp/nsc-t10-referencias-a_c7g0ym`);
- `pruebas-tanda-09-sysv-cpp-record-directo/validar-rechazos.py` pasa (`logs/regresion-t09-g1-fix.log`);
- `pruebas-tanda-14-especiales/validar.py` pasa en estática y DLL (`A:/temp/nsc-t14-especiales-a9_2fugr`,
  `A:/temp/nsc-t14-especiales-sroz3579`);
- los tres guiones de `lista-tamano-3.txt` pasan con un proceso (`logs/regresion-t09-g1-tamano-t14.log`).

El cambio solo altera el texto de ese diagnóstico, así que no se repitió la batería completa.

La regresión ejecuta cada guion sin `--dll`; las variantes DLL de los fixtures nuevos se ejecutaron
aparte (evidencias en cada registro). La sonda de la auditoría (`A:/temp/nsc-t14-sonda-9mqysi8f`) importa
todas las macros de objeto, los bit-fields C++ y el record empaquetado; rechaza con su motivo la macro de
función y el puntero a miembro, y el módulo con el diagnóstico de Clang.

## Cierre de la tanda 14 (2026-11-15)

Cada casilla T14 tiene su evidencia frente a Clang 23.1.0 en estática y DLL, O0–O3, y los pendientes que
quedan fuera están en [t14-cierre.md](registro-commits/t14-cierre.md), cada uno con la tanda o el ítem que
lo continúa. No se modificó Clang ni NSLink.
