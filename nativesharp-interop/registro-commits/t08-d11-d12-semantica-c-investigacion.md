# T08-D11 y T08-D12: qué expresiones deberían conservar la semántica entera de C

Fecha: 2026-11-09. Pendiente: T08-D11 (T08-006-04b1) y T08-D12 (T08-009-03c1).

Instrucción del usuario: no cambiar globalmente las reglas de NativeSharp. Hay que comprobar la semántica exacta
y delimitar qué es un «operando C» cuando una expresión mezcla valores C y NativeSharp.

Este documento es una investigación. No cambia el compilador.

## 1. Semántica medida

Sonda: `pruebas-tanda-08-enteros-c/sonda-semantica-c.py`. Cada caso se evalúa con un control Clang y con NSC (camino
directo, O0 y O2). El valor se amplía a 64 bits y lo imprime la misma biblioteca C en los dos programas. La sonda
falla si un valor cambia respecto de la instantánea del 2026-11-09. Con `--observar` imprime la tabla.

Valores de la biblioteca: `g_ub = 255`, `g_sb = -128`, `g_us = 65535`, `g_ss = -32768`, `g_i = -1`, `g_u = 1`,
`g_two = 2`; `g_s` es un struct con los mismos valores; `f_*()` los devuelve; `u8_t` es `typedef unsigned char`;
`enum E { EN = -1, EP = 1 }` y `g_e = EN`.

### T08-D11: `-` y `~` unarios sobre enteros estrechos

| Caso | C | NSC |
|---|---:|---:|
| `byte b = 255; ~b` (nativo) | -256 | 0 |
| `sbyte b = -128; -b` (nativo) | 128 | -128 |
| `~g_ub` (global C) | -256 | 0 |
| `-g_sb` (global C) | 128 | -128 |
| `~g_us` / `-g_ss` | -65536 / 32768 | 0 / -32768 |
| `~g_s.ub` / `-g_s.sb` (campo C) | -256 / 128 | 0 / -128 |
| `~f_ub()` / `-f_sb()` (llamada C) | -256 / 128 | 0 / -128 |
| `~g_t8` (typedef C) | -256 | 0 |
| `var t = g_ub; ~t` | -256 | 0 |
| `byte t = g_ub; ~t` | -256 | 0 |
| `~(byte)g_i` | -256 | 0 |
| `~g_ub == 0` | 0 | 1 |
| `~g_ub + b`, con `byte b = 1` | -255 | 1 |
| `-(g_sb) * b`, con `byte b = 1` | 128 | -128 |
| `-EP` (enumerador C) | -1 | -1 |
| `~(int)g_ub` / `-(int)g_sb` | -256 / 128 | -256 / 128 |

**Observaciones.**

- NativeSharp conserva el tipo estrecho del operando (`analyzeUnaryNumeric`: `result.typeKey = operand.typeKey`).
  Lo hace igual si el operando es nativo, global, campo, llamada, typedef, copia local o conversión.
- C promueve a `int`. C# también lo hace, así que la regla actual es propia de NativeSharp.
- Solo coinciden los enumeradores C, que ya son `int`, y las conversiones explícitas a `int`.

### T08-D12: `int` frente a `unsigned int`

| Caso | C | NSC |
|---|---:|---:|
| `int a = -1; uint b = 1; a < b` (nativo) | 0 | 1 |
| `g_i < g_u` (globales C) | 0 | 1 |
| `g_i < 1u` (global C y literal) | 0 | 1 |
| `-1 < g_u` (literal y global C) | 0 | 1 |
| `int a = -1; a < g_u` (local y global C) | 0 | 1 |
| `var a = g_i; a < g_u` | 0 | 1 |
| `g_s.i < g_s.u` (campos C) | 0 | 1 |
| `f_i() < f_u()` (llamadas C) | 0 | 1 |
| `EN < g_u` (enumerador C) | 0 | 1 |
| `g_e < g_u` (valor de tipo enum C) | 0 | 0 |
| `g_i / g_two` | 2147483647 | 0 |
| `g_i * g_u` | 4294967295 | -1 |
| `g_i % g_two` | 1 | -1 |
| `(g_i * g_u) >> 1` | 2147483647 | -1 |
| `g_i + g_u > 0` | 0 | 0 |
| `(uint)g_i < g_u`, `(uint)g_i / g_two`, `(uint)g_i * g_u` | 0, 2147483647, 4294967295 | iguales |

**Observaciones.**

- NativeSharp amplía `int` con `uint` a `long`, como C#. C convierte a `unsigned int`.
- Solo sigue a C un operando de tipo enum C, por la corrección de `t08-enum-c-uint-int.md`. Esa regla depende del
  tipo, no del origen: el enumerador `EN` es una constante `int` y no la activa.
- Con una conversión explícita a `uint`, NativeSharp da el valor de C.

## 2. El problema: el tipo no dice si un valor es «de C»

NativeSharp importa los enteros de C como sus propios tipos: `unsigned char` es `byte`, `unsigned int` es `uint`, y
`u8_t` también es `byte`. Un `byte` leído de una global C y un `byte` nativo tienen el mismo tipo. Por eso la
expresión no sabe hoy si un operando «es C». La única excepción es el enum C, que tiene identidad de tipo propia.

Hay tres formas de definir un operando C:

| Definición | Qué cuenta como operando C | Qué se pierde |
|---|---|---|
| **Origen directo** | lectura de una global, un campo o un elemento C; llamada a una función C; enumerador C. Se ignoran paréntesis y la conversión implícita propia | basta con copiar el valor a un local (`var t = g_ub`) para que deje de ser C |
| **Tipo C** | un valor cuyo tipo es un entero C distinto del nativo (por ejemplo `c:unsigned char`), que se conserva con `var` | el tipo se pierde al declarar un tipo nativo (`byte t = g_ub`); exige nuevos tipos, conversiones y NSMeta |
| **Contexto explícito** | toda expresión dentro de un contexto que el programador marca como C (por ejemplo `c(...)`) | nada implícito, pero es sintaxis nueva |

## 3. Expresiones mixtas

Una expresión mezcla operandos C (C) y NativeSharp (N), y los resultados intermedios también tienen que
clasificarse. Las reglas posibles son:

- **A, cualquier operando:** semántica C si algún operando de la operación es C.
- **B, todos los operandos:** semántica C solo si todos los operandos son C.

En las dos, el resultado de una operación hecha con semántica C cuenta como operando C para la operación
siguiente; sin eso, `(g_i * g_u) >> 1` mezclaría reglas.

Resultados que daría cada regla, con la definición de origen directo:

| Expresión | C | NSC hoy | Regla A | Regla B | Tipo C |
|---|---:|---:|---:|---:|---:|
| `~g_ub` | -256 | 0 | -256 | -256 | -256 |
| `var t = g_ub; ~t` | -256 | 0 | **0** | **0** | -256 |
| `byte t = g_ub; ~t` | -256 | 0 | 0 | 0 | **0** |
| `~g_ub + b` (b nativo) | -255 | 1 | -255 | -255 | -255 |
| `g_i < g_u` | 0 | 1 | 0 | 0 | 0 |
| `g_i < 1u` | 0 | 1 | 0 | **1** | depende de la regla mixta |
| `-1 < g_u` | 0 | 1 | 0 | **1** | depende de la regla mixta |
| `int a = -1; a < g_u` | 0 | 1 | 0 | **1** | depende de la regla mixta |
| `int a = -1; uint b = 1; a < b` | 0 | 1 | 1 | 1 | 1 |
| `EN < g_u` | 0 | 1 | 0 | 0 | 0 |
| `(g_i * g_u) >> 1` | 2147483647 | -1 | 2147483647 | 2147483647 | 2147483647 |

Lo que muestra la tabla:

1. **Ninguna regla reproduce C siempre.** C aplica sus reglas a todo valor. En NativeSharp, la clasificación se
   pierde en algún punto: con origen directo, al copiar a un local; con tipo C, al declarar un tipo nativo.
2. **La regla A contagia.** `a < g_u` sigue a C y `a < b` no, aunque los valores sean iguales y `b` sea una copia
   de `g_u`. Un cambio de refactorización (extraer `g_u` a un local) cambia el resultado.
3. **La regla B no contagia, pero deja fuera los literales.** `g_i < 1u` y `-1 < g_u`, que son C escrito tal cual,
   no siguen a C.
4. **El cambio no es solo de valor, también de tipo.** Con semántica C, `~g_ub` es `int`, no `byte`. Entonces:
   - `byte m = ~g_ub;` dejaría de compilar: NativeSharp no estrecha implícitamente y C sí;
   - `var r = ~g_ub;` cambiaría el tipo de `r`;
   - cambiaría la resolución de sobrecargas.

   En T08-D12, `g_i / g_two` pasaría de `long` a `uint`. Aplicar el valor de C conservando el tipo de NativeSharp
   no es coherente: sería otro operador.
5. **El precedente actual es por tipo.** El enum C con base `uint` frente a `int` sigue a C porque el tipo enum C es
   distinguible. Los enteros C no lo son.

## 4. Alternativas

| Alternativa | Qué cambia | A favor | En contra |
|---|---|---|---|
| **1. Origen directo, regla A** | `-`, `~` y operaciones `int`/`uint` con un operando C toman la semántica y el tipo de C | una expresión C copiada tal cual da el valor de C | contagia; depende de la refactorización; cambia tipos y sobrecargas; el origen debe viajar en los cuerpos precompilados |
| **2. Origen directo, regla B** | lo mismo, solo si todos los operandos son C | no contagia operandos nativos | los literales quedan fuera; mismos cambios de tipo |
| **3. Tipos enteros C distintos** | `unsigned char` de C deja de ser `byte`; sus operadores siguen a C y las conversiones a los tipos nativos se definen | coherente con el precedente del enum C; sobrevive a `var` | cambio de lenguaje grande: firmas, NSMeta, sobrecargas y genéricos |
| **4. Reglas de NativeSharp y diagnóstico** | sin cambio de semántica; aviso cuando una operación con un operando C da otro tipo o valor que en C, con la corrección `~(int)x` o `(uint)a < b` | no cambia ninguna regla; no depende de la refactorización; la corrección ya funciona (tabla 1, conversiones explícitas) | la expresión C copiada tal cual sigue dando el valor de NativeSharp hasta que se corrige |
| **5. Contexto explícito `c(...)`** | dentro del contexto, reglas de C para enteros | delimitación exacta y visible | sintaxis nueva |

**Detalles de la alternativa 4 (aviso).** Se emitiría en dos casos:

- T08-D11: `-` o `~` sobre un operando C de tipo `byte`, `sbyte`, `ushort` o `short`, donde C promueve a `int`;
- T08-D12: una comparación o una operación aritmética entre `int` y `uint` con algún operando C.

Necesita la misma marca de origen directo que la alternativa 1, pero solo para diagnosticar: no cambia tipos ni
valores.

## 5. Cómo se probaría cada alternativa

La sonda ya fija los valores de C y de NSC en 38 casos. Lo que cambiaría en cada alternativa:

| Alternativa | Cambio en la instantánea | Pruebas adicionales |
|---|---|---|
| 1 o 2 | las filas que cubre la regla pasan al valor de C | rechazo de `byte m = ~g_ub;`, tipo de `var r = ~g_ub;`, una sobrecarga `F(byte)`/`F(int)`, y los mismos casos dentro de una plantilla precompilada consumida sin encabezado |
| 3 | además, `var t = g_ub; ~t` pasa a C | conversiones `c:unsigned char` ↔ `byte` y firmas públicas en NSMeta |
| 4 | la instantánea no cambia | el aviso aparece en las filas «global», «campo», «llamada», «typedef» y mixtas, y no en las nativas ni en las conversiones explícitas |
| 5 | la instantánea no cambia | el mismo cuerpo con `c(...)` da los valores de C |

## 6. Estado

T08-D11 y T08-D12 siguen abiertas. Falta que el usuario elija una alternativa; ninguna se aplicó.

La regla vigente sigue igual y la fijan dos pruebas:

- `enteros-c/validar-anchos.py`, que usa conversiones explícitas en las comprobaciones comunes;
- `sonda-semantica-c.py`, que fija los valores actuales.
