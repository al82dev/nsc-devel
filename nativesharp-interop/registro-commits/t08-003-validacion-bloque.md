# T08-003: validación del bloque

Fecha: 2026-11-08. Todas las casillas de T08-003 están cerradas, incluida la nueva T08-003-04d1.

Batería ejecutada (`claude/scripts/validar-bloque-t08-003.cmd`). Los `.cmd` se ejecutan sobre la copia CRLF
de los árboles de prueba; ver [t08-002-validacion-bloque](t08-002-validacion-bloque.md). Todo se aprobó:

- `pruebas-tanda-08-inline-c/validar.cmd` y `validar-thinlto.cmd`;
- `pruebas-tanda-08-inline-thinlto-simbolo/validar.cmd`;
- `pruebas-tanda-08-inline-template/validar.cmd`;
- `pruebas-tanda-08-prototipos-c/validar.cmd`;
- `pruebas-tanda-08-funcion-template/validar.cmd`;
- `pruebas-tanda-08-identidad-c/sombra-funcion`, `plantilla-funcion`, `inline-dos-bibliotecas` y
  `funcion-firma`;
- `pruebas-tanda-08-funciones-c/validar-declaraciones.py`, `validar-static-encabezado.py` y
  `validar-visibilidad-enlace.py` (O0–O3).
