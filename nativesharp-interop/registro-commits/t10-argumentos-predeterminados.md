# T10-010: argumentos predeterminados C++ evaluados por Clang

## Defecto

En la [auditoría inicial](../TANDA-10-AUDITORIA-INICIAL.md), `A.d(1)` sobre `int d(int a, int b = 5)`
se rechazaba con `no overload of namespace function 'A.d' accepts 1 argument(s)`. Lo mismo pasaba
con un predeterminado `constexpr` (`b = K * 2`).

Causa en el código:

- `ClangSession.cpp` solo guardaba si un parámetro tenía valor predeterminado
  (`CppFunctionParameter::hasDefaultArgument`), no cuál era.
- `projectFunction` (`CppSemanticFunction.cpp`) no asignaba `MethodParameterSymbol::defaultValueSyntax`,
  así que la resolución de NativeSharp veía todos los parámetros como obligatorios.
- En NativeSharp, un valor predeterminado es un literal de la declaración. En la llamada lo
  materializa `lowerDefaultArgument` (`HirFunctionLowerer.cpp`), que admite literales enteros,
  flotantes, booleanos, de cadena y nulos, con signo menos.

## Cambio

- `CppFunctionDeclaration.h`: `CppFunctionParameter` guarda el tipo de predeterminado
  (`CppDefaultArgumentKind`) y sus bits. Las clases son ninguno, sin evaluar, entero con signo,
  entero sin signo, flotante, booleano y puntero nulo.
- `ClangSession.cpp` (`captureDefaultArgument`, en funciones y métodos): evalúa el predeterminado
  con `Expr::EvaluateAsRValue`, sin efectos laterales. Solo lo conserva si la constante es del tipo
  del propio parámetro:
  - un entero, con su signo, si el parámetro es entero y no enum;
  - un `bool`;
  - un flotante real, si es representable exactamente en `double`;
  - un puntero nulo.

  Cualquier otro valor queda sin evaluar: enumeradores, records, llamadas.
- `CppSemanticFunction.cpp`: `cppDefaultArgumentSyntax` crea el literal que escribiría una
  declaración NativeSharp:
  - un entero, con `-` si es negativo;
  - `true` o `false`;
  - un flotante con su representación más corta y sufijo `f` para `float`;
  - `null`.

  Los nodos viven toda la compilación, como la sintaxis de una declaración. Se asignan desde el
  último parámetro hacia atrás y se detienen en el primero sin predeterminado constante o que no
  se pasa por valor. Así los predeterminados quedan al final y un parámetro anterior a uno sin
  evaluar sigue siendo obligatorio.
- `CppCRecordMetadataCodec.cpp`: el byte que guardaba la marca guarda ahora el tipo de
  predeterminado, seguido de sus bits cuando es una constante. Es el mismo contrato NSMeta vigente,
  sin versiones nuevas.
- `CppSemanticRecord.cpp`: dos observaciones del mismo método solo son iguales si también
  coinciden sus predeterminados.

No se modificó Clang ni NSLink.

## Evidencia

Fixture nuevo `pruebas-tanda-10-predeterminados/validar.py`, con y sin `--dll`. Predeterminados:

| Función | Predeterminado |
|---|---|
| `d(int a, int b = 5)` | literal entero |
| `e(int a, int b = K * 2)` | expresión `constexpr` |
| `n(int a = -3)` | entero negativo |
| `h(double x = 2.5)` | `double` |
| `f(float x = 0.25f)` | `float` |
| `b(bool flag = true)` | `bool` |
| `p(int* q = nullptr)` | puntero nulo, también llamada con argumento |
| `u(unsigned long long v = 18446744073709551615ULL)` | máximo sin signo |
| `c(char ch = 'A')` | `char` |
| `w(long long v = -9223372036854775807LL - 1)` | mínimo con signo |
| `s(short x = 7, unsigned char y = 200)` | dos predeterminados, llamada sin ninguno y con ambos |

De O0 a O3, el fixture:

- ejecuta un control C++ compilado por Clang 23.1.0 que omite los argumentos, y un consumidor
  NativeSharp con las mismas llamadas; exige el mismo resultado (44);
- exige que las 11 declaraciones del IR de NSC tengan los tipos físicos de Clang;
- consume una plantilla precompilada, con y sin encabezado, con las mismas comprobaciones.

Controles negativos y sus contrapartes:

- `g(int a = compute())`: `T10.g()` se rechaza (`no overload … accepts 0 argument(s)`) y
  `T10.g(4)` pasa.
- `m(Mode x = High)` con un enumerador: `T10.m()` se rechaza.
- `mix(int a = 1, int b = compute())`: `T10.mix(1)` se rechaza, porque `a` va antes de un
  predeterminado sin evaluar. `T10.mix(1, 2)` pasa.

| Variante | Resultado | Evidencia |
|---|---|---|
| Estática | pasa (O0–O3, con y sin encabezado) | `A:/temp/nsc-t10-predeterminados-f_nh48wf` |
| DLL | pasa (O0–O3, con y sin encabezado) | `A:/temp/nsc-t10-predeterminados-72byk4b3` |

Regresión de las tandas 9 y 10: ver [TANDA-10-VALIDACION-AGRUPADA.md](../TANDA-10-VALIDACION-AGRUPADA.md).

## Límites

- Predeterminados de tipo enum, record o que no son constantes no se proyectan; el argumento sigue
  siendo obligatorio.
- Los métodos estáticos y de instancia guardan el valor, pero su proyección no lo usa todavía.
- Un parámetro `ref`/`in` (referencia C++) con predeterminado no lo conserva.
