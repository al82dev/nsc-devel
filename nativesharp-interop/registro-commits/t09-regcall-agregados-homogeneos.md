# T09-003: agregados homogéneos y uniones flotantes por valor con `regcall`

## Defecto

Sonda `build-claude-t08/tools/sonda-regcall-hfa.py`, evidencia
`A:/temp/nsc-t09-sonda-regcall-hfa-itucut1c` y `A:/temp/nsc-t09-sonda-regcall-hfa-mtsf_5hy`.
Records C por valor con `regcall` en dos formas:

- `f(T u, int k)`: el record como argumento;
- `g(float x)`: el record como retorno.

Cada forma se probó en llamada directa y por puntero.

| Record | Plan de Clang | `f` | `g` |
|---|---|---|---|
| `union { float a; float b; }` | `float` / `{ float }` | rechazada | rechazada |
| `struct { float a; }` | `float` / `{ float }` | rechazada | rechazada |
| `union { float a[2]; float b; }` | `float, float` / `{ [2 x float] }` | rechazada | rechazada |
| `union { double a; double b; }` | `double` / `{ double }` | rechazada | rechazada |
| `struct { double a; }` | `double` / `{ double }` | rechazada | rechazada |
| `struct { float a; float b; }` | `float, float` / `{ float, float }` | pasa | rechazada |
| `struct { float a[2]; }` | `float, float` / `{ [2 x float] }` | pasa | rechazada |
| `union { float a; int b; }` | `i32` | pasa | pasa |

En las rechazadas, la llamada directa da `regcall currently supports only int scalar signatures …`
y la llamada por puntero da `regcall function pointer signature is not yet supported`. Clang
compila todos los casos.

Causa en el código. Clang trata como agregado homogéneo un record de una a cuatro piezas del mismo
tipo flotante, y lo expande en argumentos o lo devuelve como struct LLVM. Tres puntos lo
limitaban:

- `collectExpandedPieces` (`ClangAbiPlan.cpp`) no representaba la expansión de una unión. Sin
  esas piezas no había plan.
- Los validadores de expansión de `CppRegCallAbiValidation.h` exigían de dos a cuatro piezas
  (`isRegCallWin64FloatRecordRoundTrip`, `regCallRecordMatchesExpansion`,
  `regCallRecordMatchesDoubleExpansion`). Esto excluía los records de una pieza.
- Un record homogéneo devuelto se aceptaba solo en estos casos:
  - un record `float` de 8 a 16 bytes, con al menos un record expandido entre los argumentos
    (`isRegCallWin64FloatRecordInterleaved`);
  - un record `double` de 16 a 32 bytes (`isRegCallWin64DoubleRecordInterleaved`).

  No había ruta para un record `float` devuelto desde escalares, ni para un record de una pieza.

El emisor ya era genérico: carga cada pieza en su desplazamiento y guarda el struct devuelto en el
record de NativeSharp.

## Cambio

- `collectExpandedPieces` expande una unión como su primer campo de mayor tamaño, en el
  desplazamiento cero. Es la misma regla que `getTypeExpansion` en Clang. Sigue sin aceptar:
  - un campo de bits (también de ancho cero);
  - las bases C++.

  Las piezas se siguen comparando una a una con los tipos físicos de la firma que emite Clang.
- Los validadores de expansión aceptan de una a cuatro piezas. Se mantienen sin cambios:
  - el desplazamiento de cada pieza;
  - el tipo de cada pieza;
  - el tamaño y la alineación del record.
- `isRegCallWin64HomogeneousRecordReturn` acepta un record homogéneo de una a cuatro piezas
  `float` o `double` devuelto directamente, con cualquier mezcla de escalares y de records
  expandidos, o sin ellos. Para esto, `isRegCallWin64FloatRecordRoundTrip` recibe
  `allowSinglePieceReturn`, que admite el record de 4 bytes (`float`) o de 8 bytes (`double`).
- El validador nuevo acompaña a `isRegCallWin64DoubleRecordInterleaved` en sus 15 usos:
  - la HIR;
  - el catálogo NSMeta (`CppCRecordMetadataCodec.cpp`);
  - las sesiones y el inventario C++;
  - las declaraciones, las llamadas y los punteros del emisor.

  En la HIR va en el grupo que concilia la identidad USR, el tamaño y la alineación del record
  devuelto con el tipo importado.

No se modificó Clang ni NSLink. Los planes viajan en los campos existentes y el contrato NSMeta
vigente no cambia.

## Evidencia

### Sonda tras el cambio

`A:/temp/nsc-t09-sonda-regcall-hfa-yfazc079`: los ocho records pasan en las cuatro celdas
(argumento y retorno, en llamada directa y por puntero).

### Fixture nuevo

`pruebas-tanda-09-regcall-hfa-uniones/validar.py`, con y sin `--dll`, siempre con `--precompilado`.

Records:

- uniones de `float` y de `double`;
- una unión con un arreglo como campo mayor;
- una unión de tres piezas (un struct como campo mayor) y otra de cuatro;
- structs de un `float` y de un `double`;
- un struct de dos `float`.

Firmas, en llamada directa y por puntero:

- el record en primera posición, entre escalares o junto a otro record, con retorno `int`, `float`
  o `double`;
- siete funciones que devuelven esos records a partir de escalares o de otro record.

De O0 a O3, el fixture:

- ejecuta un control C compilado por Clang 23.1.0 y el consumidor NativeSharp con las mismas
  comprobaciones;
- exige en cada declaración del IR de NSC la convención `x86_regcallcc` y los tipos físicos de
  Clang: cada parámetro y el retorno, con los structs con nombre de Clang sustituidos por su
  definición;
- exige la convención en cada llamada directa y en las 15 llamadas por puntero;
- consume una plantilla precompilada con y sin encabezado.

El control negativo comprueba un struct C++ `{ float a; int : 0; float b; }`. Clang omite el campo
de bits de ancho cero y expande dos `float`, y el fixture comprueba esa expansión en el IR de
Clang. NSC no representa campos de bits en una expansión, así que lo sigue rechazando con
`regcall currently supports only int scalar signatures`.

| Variante | Resultado | Evidencia |
|---|---|---|
| Estática + precompilado | pasa (O0–O3) | `A:/temp/nsc-t09-regcall-hfa-5j4307ef` |
| DLL + precompilado | pasa (O0–O3) | `A:/temp/nsc-t09-regcall-hfa-arzg4n9r` |

### Controles negativos convertidos

La unión C solo de `float` con `regcall` era el control negativo de cinco guiones y ahora se
admite. Siguiendo la política acordada, cada guion la compara con Clang:

- de O0 a O3, un control C de Clang da el resultado esperado;
- NSC declara y llama el símbolo con `x86_regcallcc` y la unión como `float`;
- el ejecutable de NSC devuelve el mismo resultado.

Cada guion conserva el rechazo con el struct C++ con campo de bits de ancho cero, con el mismo
diagnóstico. En cuatro guiones el cambio lo aplicó `build-claude-t08/tools/transformar-union.py`.
En `pruebas-tanda-09-c-union-convenciones/validar.py` se cambió a mano: la firma con la unión solo
`float` deja de excluir `regcall` y entra en la comparación general del fixture, que incluye el IR,
los punteros y la plantilla precompilada.

| Guion | Evidencia |
|---|---|
| `pruebas-tanda-09-regcall/validar-negativo.py` | `A:/temp/nsc-t09-regcall-negativo-lv_j3dv1` |
| `pruebas-tanda-09-regcall-dos-records/validar.py` | `A:/temp/nsc-t09-two-records-sqstf_bd` |
| `pruebas-tanda-09-c-enum-convenciones/validar.py` | `A:/temp/nsc-t09-c-enum-convenciones-q5i129bw` |
| `pruebas-tanda-09-cpp-enum-convenciones/validar.py` | `A:/temp/nsc-t09-cpp-enum-convenciones-u0nj7vzt` y, con `--precompilado`, `A:/temp/nsc-t09-cpp-enum-convenciones-ttbefm0g` |
| `pruebas-tanda-09-c-union-convenciones/validar.py`, `--precompilado` | `A:/temp/nsc-t09-c-union-convenciones-ursa1zdq` (estática) y `A:/temp/nsc-t09-c-union-convenciones-6letbmbq` (DLL) |

Pasan los cinco guiones (`logs/regresion-t09-uf-convertidos/` y `logs/fx-union-*.log`).

Regresión de la tanda: ver [TANDA-09-VALIDACION-AGRUPADA.md](../TANDA-09-VALIDACION-AGRUPADA.md).

## Límites

- Los campos de bits, también los de ancho cero, siguen fuera de la expansión.
- Las bases C++ siguen fuera de la expansión.
- Las uniones C++ no se importan como tipos de valor: `T09Z` no tiene miembros accesibles desde
  NativeSharp (`sonda-hfa-neg.py`, `A:/temp/nsc-t09-sonda-hfa-neg-4jfuipc9`). Es un límite previo,
  fuera de este cambio.
- Los records C++ por valor en punteros `regcall` siguen rechazados.
