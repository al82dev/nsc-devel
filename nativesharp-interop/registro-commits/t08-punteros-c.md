# T08-014: punteros C

Fecha: 2026-11-08. Casillas: T08-014-02a–d, 03a–c y 04a–c. También incluye la validación del bloque T08-014.

## Resultado

Verificadas, con una corrección en el compilador: los campos `T **` de base escalar. Quedan la limitación de
representación documentada en 02c y la decisión pendiente T08-014-02d1.

## Corrección: campo `int **`

**Síntoma.** En `struct Links { …; int **ipp; }` el campo no se importaba:

- en el camino directo, «has no compatible member named 'ipp'»;
- en el camino precompilado, «C record field cannot be reconstructed: PtrPub::Links.ipp».

Un parámetro `int **` sí funcionaba.

**Causa.** Es la misma que la de `Record **` (T08-013): `projectCppSemanticInstanceField` solo proyectaba un nivel
de puntero.

**Corrección.** Con punteros intermedios sin calificar y una base escalar sin calificar, la clave conserva el tipo
escalar y agrega un `*` por cada nivel.

## Prueba

Nueva: `pruebas-tanda-08-punteros-c/validar-punteros.py`. Aprobada en O0–O3, con los rechazos en O0.

El mismo cuerpo se ejecuta en un control Clang, en el camino directo y en una plantilla precompilada consumida sin
encabezado (03c).

- **02a / 03a — punteros a escalares.** Enteros de 8 a 64 bits con y sin signo, `float`, `double` y `_Bool`, como
  parámetro y retorno de funciones C que escriben a través de ellos.
- **02b — otros apuntados.** Punteros a struct, union, enum y `void`. Incluye `void*` explícito a `int*`.
- **02c — dos niveles.** `int **` como parámetro y campo. En el lado NativeSharp, `int const**`, y un puntero a
  `volatile` obtenido de C con `var`.
  - **Límite de representación.** Un parámetro C con calificador en el segundo nivel (`const int **`,
    `volatile int **`) o en un puntero intermedio (`int *const *`) no se importa. La función queda diferida con el
    diagnóstico explícito «C/C++ function '…' cannot be imported: C/C++ parameter 0 is not yet mapped», y la
    prueba lo fija.
- **02d — conversiones.**
  - Agregar `const` o `volatile`, y pasar de `T*` a `void*`, es implícito.
  - Quitar `const` se rechaza: «would discard pointee constness».
  - Quitar `volatile` se rechaza: «would discard pointer pointee volatile qualification».
  - Pasar de `void*` a `int*` exige cast: «cannot initialize … from 'builtin:void*'».
- **03b — campos y globales.** Campos puntero (`int*`, `const char*`, `Pt*`, `void*`, `int**`) y globales C
  `int*` y `const char*`. Se leen, se reasignan y C los comprueba.
- **04a — dirección de campos.**
  - Se admite la de un campo natural y la de un campo `char` de un struct `pack(1)`.
  - Se rechaza la de un `int` desalineado del struct empaquetado: «operand of & must name addressable storage».
    El texto del diagnóstico menciona campos de bits, pero el rechazo es el del contrato vigente.
- **04b — nulos.**
  - `null` hacia C, un puntero nulo devuelto por C, y la comparación con `null`.
  - Desreferenciar `void*` o un entero se rechaza: «operand of * must be a non-void pointer».
- **04c — calificadores al inicializar y reasignar.**
  - Reasignar un `int const*` o un puntero a `volatile` se admite, y se conserva la calificación.
  - Escribir por `int const*` se rechaza.
  - En NativeSharp, `const int*` declara un local constante: reasignarlo se rechaza.

## Decisión pendiente T08-014-02d1

`int* p; int const** q = &p;` se acepta en NativeSharp. En C es la conversión insegura clásica: permite guardar un
`const int*` en `p` y luego escribir por `p`. Clang solo avisa en C y C++ la rechaza.

**Opciones.**

1. Rechazarla en el sistema de tipos de NativeSharp. Es un cambio del lenguaje, no de interop.
2. Mantenerla como está.

## Validación del bloque

- La prueba nueva.
- La muestra previa T08-014-01: `bool-puntero-c/validar.cmd`, en la regresión de records.
- La batería completa con el compilador final.
