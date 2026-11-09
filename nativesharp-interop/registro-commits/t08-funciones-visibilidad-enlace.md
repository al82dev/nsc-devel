# T08-003-05: visibilidad y enlace de funciones C

Fecha: 2026-11-08. Casillas: T08-003-05a, 05b y 05c.

## Resultado

Todas las casillas se verificaron sin cambios en el compilador.

## Prueba

Nueva: `pruebas-tanda-08-funciones-c/validar-visibilidad-enlace.py`. Resultado: aprobada en O0–O3, con
rechazos en O0. Un control Clang hace las mismas llamadas con las mismas bibliotecas.

**05a — Llamada desde un namespace y local homónimo.**
- Las funciones C se llaman desde un método de clase y desde una función de namespace, ambos dentro de
  `namespace App.Inner`.
- Un local `shadow_me`/`dll_shadow` oculta la función C dentro de su bloque. Al salir del bloque, la
  llamada vuelve a resolver la función.
- Llamar al local como si fuera función se rechaza con «call target is not an instance method group».

**05b — Enlace con biblioteca estática y con DLL.**
- `add_c`/`shadow_me` vienen de una biblioteca estática.
- `dll_mul`/`dll_shadow` vienen de una DLL con `__declspec(dllimport)`. La DLL se publica junto al ejecutable,
  que se ejecuta desde otra carpeta.
- Una función declarada sin definición se rechaza al enlazar con «unresolved external symbol:
  'undefined_c'».

**05c — Plantilla precompilada, con el encabezado visible y oculto.**
- La plantilla llama a las funciones de las dos bibliotecas y declara un local homónimo.
- Se consume con los encabezados referenciados en el consumidor, y también con los encabezados ocultos:
  las bibliotecas llegan por la metadata del productor.
- Las dos formas dan 520 en O0–O3.

Esto amplía las muestras previas de `pruebas-tanda-08-identidad-c/sombra-funcion` y
`plantilla-funcion`, que siguen en el validador del bloque T08-002.
