# T08-009: enumeraciones C

Fecha: 2026-11-08. Casillas: T08-009-03a–d, T08-009-04a–d y T08-009-05a–c.

## Defectos corregidos

### 1. Enum C de base `int` comparado u operado con `unsigned int` (03c)

- **Síntoma.** Con `Range r = RNeg;` (-1), NativeSharp daba `r < 1u` verdadero y `r / 2u == 0`. Clang da
  falso y 2147483647, porque aplica las conversiones aritméticas usuales: el `int` pasa a `unsigned int`.
- **Causa.** `cppCEnumMixedUintInt` (`CppImportedEnumNumeric.h`), introducido para el caso inverso (enum
  de base `unsigned int` con `int`, `t08-enum-c-uint-int.md`), solo reconocía ese orden de signos.
- **Corrección.** La función reconoce las dos combinaciones de 32 bits con signo opuesto. Los dos puntos que
  ya la usan, el análisis aritmético y los operandos numéricos de HIR, eligen `unsigned int`.

### 2. Asignación compuesta sobre un objeto enum C (03d)

- **Síntoma.**
  - `Flags f = FA; f |= FB;` se rechazaba con «compound assignment requires compatible numeric operands»,
    aunque C lo admite.
  - Tras permitirlo, `f <<= 1` fallaba al bajar a LLVM con «shift operands must have concrete integer
    widths».
- **Causa.**
  - En `finishNumericCompoundAssignment` (`AssignmentAnalyzer.cpp`), un enum C solo admitía `op=` sobre
    globales atómicas.
  - `compoundUpdatedValue` (`OopLlvmArraysAndStores.cpp`) tomaba el ancho, el signo del desplazamiento y la
    división del tipo enum, que no es un entero concreto.
- **Corrección.**
  - Un objeto enum C admite `op=` con un entero o con otro enum C. El tipo común se calcula con los tipos
    subyacentes y el resultado se guarda en el enum, como en C.
  - La bajada de un `op=` sobre enum usa el entero subyacente para la operación y el desplazamiento.

## Pruebas

Nuevas en `pruebas-tanda-08-enum-c/`. Ambas aprobadas: casos positivos en O0–O3, rechazos en O0.

- `validar-valores.py` (03a–d). El mismo cuerpo se ejecuta en un control Clang, directo y en una plantilla
  precompilada consumida sin encabezado.
  - **03a — límites.**
    - `INT_MIN`, -1 e `INT_MAX`.
    - Bases fijas `unsigned int`, `unsigned char` y `long long`.
    - Un enum con 0xFFFFFFFF sin base fija: en el target MSVC sigue siendo `int` (-1), igual que en Clang.
    - Vuelta de `INT_MAX` a `INT_MIN` en C.
    - Tamaños según Clang (4, 4, 1 y 8).
  - **03b — conversiones.** Explícitas e implícitas entre enum, `int`, `unsigned int`, `long` y `ulong`.
    `small_make(300)` da 44.
  - **03c — comparaciones mixtas.**
    - Un operando de tipo enum `int` frente a `unsigned int` (en los dos órdenes, y en suma y división).
    - Un operando de tipo enum `unsigned int` frente a `int`.
    - Base `unsigned char` frente a `int`, y base `long long`.
  - **03d — operaciones.**
    - `|`, `&`, `^`, `~`, `<<`, `>>`, `+`, `-`, `*`, `%` y `-` unario sobre enumeradores.
    - `op=` sobre un objeto enum: `|=`, `&=`, `^=`, `<<=`, `+=`, `-=` y `>>=`, con el resultado comprobado
      por C.
  - **Rechazos.** NativeSharp es más estricto que C en dos casos:
    - enum a `bool`: «cannot initialize local 'b' of type 'builtin:bool' from 'builtin:int'»;
    - `f += 1.5`: «compound assignment requires compatible numeric operands».
- `validar-identidad.py` (04a–d, 05a–c). Los valores se comparan con funciones compiladas por Clang desde
  cada encabezado.
  - **04a.**
    - `enum Mood` igual en dos encabezados, y en un encabezado común con guardas, es un solo enum.
    - Con otro valor, el proyecto compila mientras no se usa, como las dos unidades de traducción de C.
    - Usar `Happy` se rechaza: «cannot import C/C++ global 'Happy': conflicting C enumerator
      declarations».
  - **04b.** Dos enums anónimos con el mismo primer enumerador y el mismo valor se aceptan. Con otro valor,
    `First` se rechaza con el mismo diagnóstico y los demás enumeradores siguen disponibles.
  - **04c.** Un enum anónimo como tipo del campo de un record publicado: la plantilla lo escribe, lo compara
    y C lo lee.
  - **04d.** `enum Inner`, declarado dentro de `typedef struct Outer`, se usa en una plantilla consumida sin
    encabezado con su record y sus valores.

    Con un struct declarado solo con etiqueta, el consumidor no encuentra el record. Es el hallazgo ya
    registrado como T08-010-10.
  - **05a.** Una firma pública no genérica con `Color`, llamada desde un consumidor que tiene el encabezado.
  - **05b.** La misma firma con el encabezado oculto, incluidos los enumeradores (`Red`, `Green`) tomados
    de la metadata.
  - **05c.** Un consumidor cuyo encabezado da otros valores a `Color` se rechaza: «C/C++ type alias
    conflicts with a native type 'Color'».

Regresión: `regresion-enum.cmd`, que incluye:

- en `pruebas-tanda-08-typedef-c`: `validar.cmd`, aritmética, enum/entero, mixto y enumerador;
- en `pruebas-tanda-08-enum-funcion`: el validador y el callback precompilado;
- los atómicos de `pruebas-tanda-08-tls-c` (enum atómico compuesto);
- en `pruebas-tanda-08-globales-c`: atómicas y enum/record;
- `pruebas-tanda-08-enteros-c`.

También se ejecutaron el validador de identidad desde la copia CRLF y `pruebas-tanda-08-tls-c/atomic/validar.cmd`.
Este último falla en su paso de caché antes de llegar a los atómicos: espera que `--artifact-cache` rechace una
referencia C («C/C++ project references require --no-artifact-cache …»). El commit anterior `03bf12e42`
(caché de encabezados C directos) retiró ese mensaje, así que la expectativa quedó obsoleta y no la causa este
cambio. Sin ese paso, todos los atómicos pasan, incluidos el enum atómico compuesto y O0–O3. Queda anotado para
la validación integral.

## Decisión pendiente

- **T08-009-03c1: `int` frente a `unsigned int`.**
  - Los enumeradores C son constantes `int`, como en C.
  - NativeSharp compara `int` con `unsigned int` ampliando a `long` (`-1 < 1u` es verdadero), mientras C
    convierte a `unsigned int` (falso). Ocurre igual con un `int` cualquiera, y con `UBig > -1` cuando la
    constante es de base `unsigned int`.
  - Solo un operando de tipo enum C sigue la regla de C.
  - Cambiarlo es una regla del lenguaje, no de interop. La prueba fija el comportamiento actual.
