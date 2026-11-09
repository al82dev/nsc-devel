# T08-007: punto flotante C

Fecha: 2026-11-08. Casillas: T08-007-02a–c y T08-007-03a–c.

## Defecto corregido (02c)

- **Síntoma.** Una función C cuya firma usa un tipo flotante sin portador en NativeSharp (`_Float16`) se
  descartaba en silencio. Su llamada daba solo «unknown identifier 'h_param' in callable/initializer
  context», sin el motivo. Una global del mismo tipo sí se diagnosticaba («cannot import C/C++ global 'g_h':
  C/C++ global type is not yet mapped»).
- **Causa.** La selección de funciones C ya conserva como candidatas diferidas las declaraciones que no se
  pudieron proyectar, con su motivo. Sin embargo, `CallAnalyzer` solo informaba ese motivo en dos casos:
  - cuando la llamada tenía otras sobrecargas proyectadas («C/C++ overload set is incomplete»);
  - cuando había declaraciones en conflicto.

  Una función única no proyectada caía al «unknown identifier» genérico.
- **Corrección.** En una llamada directa no calificada cuya única candidata es una función C/C++ diferida
  sin conflicto, el diagnóstico conserva el texto «unknown identifier '…' in callable/initializer
  context» y añade «: C/C++ function '…' cannot be imported: <motivo>». Por ejemplo: «C/C++ return type is
  not yet mapped».

## Prueba

Nueva: `pruebas-tanda-08-flotante-c/validar-formatos.py`. Resultado: aprobada en O0–O3, con rechazos en O0.

El mismo cuerpo se ejecuta en un control Clang, directo con el encabezado y en una plantilla de un productor
precompilado consumida sin encabezado.

- **02a/02b — `float` y `double`.**
  - Tamaños 4 y 8, con `FLT_MANT_DIG` y `DBL_MANT_DIG` comprobados en Clang.
  - Patrones de bits leídos por C para valores producidos por cada lado:
    - `0.1f` da `0x3DCCCCCD` y `0.1` da `0x3FB999999999999A`;
    - `-0.0f`, como literal y como retorno de C, da `0x80000000`;
    - el menor subnormal, como literal y desde C, da `1`;
    - infinito y NaN (NaN distinto de sí mismo).
  - Construcción desde bits (`1.0f`, `2.0`).
  - Redondeo de `float` en `16777216.0f + 1.0f`.
- **02c — `long double`.**
  - En el target fijado es IEEE binary64: tamaño 8 y `LDBL_MANT_DIG == 53` en Clang. Sus bits coinciden con
    los de `double` (`ld_bits(0.1)`), en parámetros, retorno, campo y global.
  - Un tipo sin portador se diagnostica en lugar de aproximarse, en global, parámetro y retorno.
  - Un `long double` que no sea binary64 no puede aparecer en este target. La regla de correspondencia que lo
    rechazaría la fija `CppClangSessionTests` («equal width alone does not establish floating-point
    representation»).
- **03a — parámetros y retornos.** Suma de cada tipo y una firma mixta con `int`, `long long`, `float`,
  `double` y `long double`. Además, ocho argumentos flotantes, de modo que parte va por la pila según la ABI
  de Win64.
- **03b — campos y globales.**
  - Struct con `float`, `double`, `long double`, `char` y `float[2]`; offsets 16 y 28 iguales a Clang.
  - C llena el struct y NativeSharp lo lee; luego NativeSharp escribe y C lo comprueba, incluido el signo de
    `-0.0f` en el arreglo.
  - Lo mismo con tres globales.
- **03c — metadata precompilada.** El productor publica la plantilla y el consumidor la ejecuta sin
  encabezado. El formato de `long double` viaja en el catálogo y la plantilla lo usa en parámetros, retorno,
  campo y global.
- **Rechazos.**
  - `_Float16` en parámetro, en retorno y en global.
  - `double` a `float` sin conversión explícita: «cannot initialize local 'f' of type 'builtin:float' from
    'builtin:double'».

Regresión: `regresion-llamadas.cmd`, que incluye:

- el validador previo de `pruebas-tanda-08-flotante-c`;
- `pruebas-tanda-08-funciones-c` (declaraciones, `static` en encabezado y visibilidad), O0 y O3;
- `regresion-inline.cmd`, `regresion-typedef.cmd` y el validador de identidad desde la copia CRLF.
