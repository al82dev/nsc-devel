# T08-004: validación del bloque

Fecha: 2026-11-08.

## Estado de las casillas

Todas las casillas de T08-004 están cerradas, salvo estas:
- T08-004-04a1: decisión pendiente. Permitir `&` de una global C `const`; pruebas existentes fijan el
  rechazo actual.
- T08-004-04d1: decisión pendiente. Elegir entre dos diseños para `_Atomic` sobre un record.
- T08-004-05: se ejecuta dentro de la validación integral de la tanda 8.

## Batería ejecutada

`claude/scripts/validar-bloque-t08-004.cmd`, con los `.cmd` sobre la copia CRLF. Todo se aprobó:

- `pruebas-tanda-08-global-static` y sus variantes: `agregados`, `ambiguedad`, `complejas`, `contratos`,
  `matriz`, `punteros`, `qualified`, `template`, `tipos`, `tls` y `tls-tipos`;
- `pruebas-tanda-08-global-template`, `global-puntero-const` y `bool-global-c`;
- `pruebas-tanda-08-atomic-global`, `atomic-global-dll`, `atomic-global-matrix` y `atomic-enum`;
- `pruebas-tanda-08-tls-c`: `validar.cmd`, `validar-dll.cmd`, `scalars`, `atomic`, `array`, `pointer`,
  `static-global` y `precompiled`;
- `pruebas-tanda-08-globales-c` (nuevas, O0–O3): `validar-escalares-calificados.py`,
  `validar-enum-record.py`, `validar-arreglos.py`, `validar-atomicas.py`, `validar-tls.py` y
  `validar-consumo.py`.
