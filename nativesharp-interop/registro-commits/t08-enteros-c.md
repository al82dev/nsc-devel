# T08-006: enteros C y `_Bool`

Fecha: 2026-11-08. Casillas: T08-006-03a–e y T08-006-04a–d. También incluye la validación del bloque
T08-006.

## Resultado

Verificadas sin cambios en el compilador. Queda una decisión pendiente: T08-006-04b1.

## Prueba

Nueva: `pruebas-tanda-08-enteros-c/validar-anchos.py`. Resultado: aprobada en O0–O3, con rechazos en O0.

Una sola biblioteca C cubre los once tipos enteros del target fijado (x86_64-pc-windows-msvc). El mismo cuerpo
se ejecuta en tres caminos:

- un control Clang;
- directo con el encabezado;
- una plantilla de un productor precompilado consumida sin encabezado.

Lo que se verifica:

- **03a–03d.** Para cada tipo:
  - tamaño según Clang (`sizeof` en C) y según NativeSharp;
  - límites de `limits.h` comprobados por C al recibirlos de vuelta;
  - parámetro y retorno;
  - vuelta de máximo a mínimo tras convertir en C;
  - offset dentro de un struct con todos los tipos;
  - global y elemento de arreglo.

  Equivalencias en este target:

  | C | NativeSharp |
  |---|---|
  | `char` (con signo) | `sbyte` |
  | `long` / `unsigned long` (32 bits) | `int` / `uint` |
  | `long long` / `unsigned long long` | `long` / `ulong` |
- **03e.** `_Bool` en firmas (`not_b`, `widen_b`) y en memoria. Ocupa un byte, y C lee 1 o 0 crudos cuando
  NativeSharp escribe `true` o `false`, en campo y en global.
- **04a.** Extensión: C ensancha a `long long` o `unsigned long long` el mínimo y el máximo recibidos, y
  NativeSharp ensancha igual los retornos estrechos.
- **04b.** Promociones de operadores binarios (`+`, `*`, `<<`, `>>`, `&`, `|`, `^`) sobre `byte`, `sbyte`,
  `ushort` y `short`. Los valores coinciden con los que calcula Clang.

  `_Bool` llega como `bool` y no entra en la aritmética sin una conversión explícita. Su promoción a `int`
  se comprueba en el lado C (`widen_b`).
- **04c.** Lectura y escritura cruzadas: C llena struct, globales y arreglos y NativeSharp los lee; luego
  NativeSharp escribe otros valores y C los comprueba.
- **04d.** Conversiones con signo y sin signo en los límites: `(uint)INT_MIN`, `(int)UINT_MAX`,
  `(byte)SCHAR_MIN`, `(sbyte)UCHAR_MAX`, `(long)ULLONG_MAX` y otras. También los literales `ULLONG_MAX` y
  `LLONG_MIN`.
- **Rechazos.** Estrechar `int` a `sbyte`, cambiar de `int` a `uint` y convertir `int` a `bool` exigen
  conversión explícita: «cannot initialize local 'v' of type 'builtin:…' from 'builtin:int'».

Validación del bloque: la prueba nueva y las de T08-006-01 y 02 (`pruebas-tanda-08-bool-callback-c`, dentro
de `regresion-callbacks.cmd`), aprobadas con el mismo compilador.

## Decisión pendiente

- **T08-006-04b1: `-` y `~` unarios sobre enteros estrechos.**
  - En NativeSharp, el resultado conserva el tipo del operando (`analyzeUnaryNumeric`:
    `result.typeKey = operand.typeKey`). En C, y también en C#, el operando se promueve a `int`.
  - Con `byte ub = 255` y `sbyte sb = -128`, NativeSharp da `~ub == 0` y `-sb == -128`. C da -256 y 128.
  - Las constantes enum C ya se promueven (`shiftResultType`). Los enteros importados de C se tratan como
    tipos NativeSharp.
  - Una expresión C copiada tal cual cambia de valor. Cambiarlo es una regla del lenguaje, no de interop.
  - La prueba fija el comportamiento actual y usa `~(int)ub` y `-(int)sb` en las comprobaciones comunes.
