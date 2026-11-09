# T08-004-06: consumo y resolución de globales C

Fecha: 2026-11-08. Casillas: T08-004-06a, 06b, 06c y 06d.

## Resultado

Verificadas sin cambios en el compilador.

## Prueba

Nueva: `pruebas-tanda-08-globales-c/validar-consumo.py`. Resultado: aprobada en O0–O3, con rechazos en O0.

- **06a — Consumidor con el encabezado.** La plantilla del productor y el código propio del consumidor usan
  el mismo objeto:
  1. la plantilla incrementa la global y el consumidor lee 13;
  2. el consumidor escribe 20;
  3. la plantilla y una función C leen 20.
- **06b — Encabezado oculto.** La misma plantilla se consume solo con la metadata.
- **06c — Ambigüedad y sombra.**
  - Dos encabezados del mismo proyecto declaran `dup` como `int` y como `long long`. El uso se rechaza con
    «cannot import C/C++ global 'dup': incompatible C/C++ global declarations».
  - Un local de bloque, un parámetro y la variable de un `for` ocultan la global solo en su ámbito. Fuera de
    él se lee y escribe la global C.
- **06d — Globales `static` con encabezados homónimos.**
  - Dos productores tienen `same.h` con `static int counter` (100 y 200), y cada plantilla incrementa el
    suyo. Los valores son independientes: 101/201 y luego 102/202.
  - Un proyecto que ve los dos encabezados se rechaza: «cannot import C/C++ global 'counter': ambiguous C
    internal global origins».

Estas casillas también tienen evidencia previa en el validador del bloque T08-002:
`homonimos/validar-global-static.cmd`, `static-dos-bibliotecas`, `externa-dos-bibliotecas` y
`global-externa-variantes`.
