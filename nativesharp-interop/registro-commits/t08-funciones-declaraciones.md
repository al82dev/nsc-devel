# T08-003-04: declaraciones y redeclaraciones de funciones C

Fecha: 2026-11-08. Casillas: T08-003-04a, 04b, 04c y 04d.

## Resultado

Todas las casillas se verificaron sin cambios en el compilador. El comportamiento actual coincide con C
(control Clang) en los casos positivos y rechaza con un diagnóstico propio los incompatibles.

## Prueba

Nueva: `pruebas-tanda-08-funciones-c/validar-declaraciones.py`. Resultado: aprobada. Los casos positivos
corren en O0–O3 y los rechazos en O0. La opción `--observar` imprime el resultado de cada caso sin asertarlo.

**04a — Prototipo compatible entre declaración adelantada y definición.** Hay un control Clang con las
mismas llamadas, directo y desde una plantilla precompilada consumida sin el encabezado. Casos:
- declaración repetida;
- parámetro escrito con typedef;
- `const` superior del parámetro;
- arreglo `int[]` e `int[3]` ajustado a `int*`;
- parámetro función `int f(int)` ajustado a puntero, al que se pasa una función C importada;
- declaración sin prototipo completada por otra posterior;
- `static inline` declarada y definida después.

**04b — Redeclaraciones incompatibles.**

| Caso | Diagnóstico |
|---|---|
| Un encabezado: otro retorno, otro parámetro u otra aridad | error de Clang «conflicting types for 'bad'» |
| Dos encabezados de un proyecto: otro retorno, otro parámetro o variádica frente a fija | «conflicting declarations for one Clang USR» |
| Plantillas de dos productores precompilados (`int split(int)`/`long split(int)`), sin encabezados | «conflicting C function declarations across precompiled library and header» |
| Llamada con aridad distinta del prototipo compuesto | «no overload of namespace function 'composite' accepts 1 argument(s)» |

**04c — Funciones variádicas y llamadas sin prototipo con promociones por defecto.**
- Sin prototipo, con definición K&R en tipos promovidos: `sbyte`→`int`, `float`→`double`, `short`→`int`,
  `ushort`→`int` y un puntero.
- Variádica, directa y precompilada:
  - `sbyte`, `short`, `bool`, `ushort` y `byte` leídos como `int`;
  - `float` leído como `double`;
  - `long`, un puntero y un record C por valor.
- Rechazos: un `struct` NativeSharp en una posición variádica o sin prototipo («no viable overload»). No
  tiene tipo C.

**04d — `static inline` de encabezados distintos.**
- Dos encabezados de un proyecto, O0–O3. Cada `outer_x`/`outer_y` llama a su propio `helper`. Cada
  contador `static inline` conserva su variable local `static` independiente (1, 2 y 101).
- Plantillas de dos productores precompilados, consumidas sin encabezados, O0–O3. Las dos tienen un `outer`
  homónimo que llama a un `helper` homónimo con otro cuerpo. Cada plantilla conserva su función y su
  ayudante (30040 = 30·1000 + 40).
- La ambigüedad de dos `static inline` homónimas usadas por el mismo proyecto sigue cubierta por
  `pruebas-tanda-08-identidad-c/homonimos` (InlineAB/InlineAC).

## Hallazgo registrado

Una función C `static` sin `inline` definida en un encabezado (`static int f(void) { … }`) no se importa: el
uso da «unknown identifier». En C es válida y equivale a una copia por unidad de traducción. Se registra
como T08-003-04d1.
