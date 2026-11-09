# T08-D17: rechazo de `int const** q = &p`; `T const**` con el significado de C

Fecha: 2026-11-09. Casillas: T08-D17 y T08-014-02d1
([decisión anterior](t08-punteros-c.md#decisión-pendiente-t08-014-02d1)).

## Problema

`int* p; int const** q = &p;` se aceptaba. En C es la conversión insegura clásica: C avisa y C++ la rechaza. Si
se acepta, `*q = c` puede guardar un `int const*` en `p`, y luego se escribe el dato constante mediante `p`.

## Causa raíz

NativeSharp tiene un solo calificador de apuntado por puntero (`pointerPointeeReadonly`, de la sintaxis
`T const*`). En un puntero a puntero, ese calificador se aplicaba al primer nivel. Por eso `int const**` significaba
lo que en C se escribe `int* const*`, y no `const int **`. Con esa lectura:

- `int const** q = &p` era seguro, porque no podía guardar nada mediante `*q`;
- `*q = c` se rechazaba («not assignable»);
- `**q = 3` se aceptaba, aunque el dato fuera constante (`int const* c; int const** q = &c;`);
- `int** r = &c`, con `c` de tipo `int const*`, se aceptaba y perdía el `const` del dato.

## Decisión aplicada

Se aplica la opción 1: rechazar la conversión en el sistema de tipos de NativeSharp. Para que el rechazo tenga
sentido, `T const**` pasa a significar lo mismo que en C: el calificador es del tipo base.

| Expresión, con `q` de tipo `int const**` | Antes | Ahora |
|---|---|---|
| `*q = c` (`c` es `int const*`) | rechazada | aceptada: `*q` es un `int const*` modificable |
| `**q = 3` | aceptada | rechazada: el dato es constante |
| `int* m = *q` | aceptada | rechazada: pierde `const` |
| `q[i]` | slot de solo lectura | `int const*` modificable, como `*q` |
| `&c` con `c` de tipo `int const*` | `int**` con el primer nivel de solo lectura | `int const**` |
| `int** r = &c` | aceptada | rechazada: pierde `const` |
| `int const** q = &p` (`p` es `int*`) | aceptada | rechazada (D17) |

Un `T**` no se convierte implícitamente en `T const**` en ninguna forma: inicialización, asignación, llamada
(NativeSharp, C o constructor), retorno e inicializador de campo. Tampoco con tres niveles. El escape es un cast
explícito: `(int const**)&p`. NativeSharp no puede escribir `int const* const*`, que C++ sí admite.

Además:

- Un condicional o `??` entre punteros a puntero con distinta calificación no tiene tipo común y se rechaza.
- La dirección de un puntero de solo lectura (un parámetro `in`, un slot `int * const` de C) se rechaza. Sería un
  `int* const*`: un calificador en un puntero intermedio, que NativeSharp no representa. Antes daba un `int**` con el
  primer nivel de solo lectura.
- `volatile` sigue sin aplicarse a punteros anidados.
- Las importaciones C con calificadores anidados (`const int **`, `int * const *`) siguen sin importarse. Lo fija la
  prueba de punteros, así que no hay interpretaciones mezcladas.
- La bajada a LLVM no usa estos calificadores, así que el código generado no cambia.

## Cambio

- `MethodBodyInternals.h/.cpp`: `nestedPointerTypeKey`, `addsNestedPointeeConst` y el mensaje común.
- `IdentifierAnalyzer.cpp`:
  - `&` de un puntero: el resultado conserva el calificador base del operando, y un puntero de solo lectura se
    rechaza;
  - `*` de un puntero a puntero: el resultado es modificable y lleva el calificador.
- `IndexAndRangeExpressions.cpp`: `q[i]` sobre un puntero a puntero, igual que `*q`.
- `StatementAnalyzer.cpp` (inicialización y retorno), `AssignmentAnalyzer.cpp`, `CallAnalyzer.cpp` (las dos
  evaluaciones de argumentos), `MethodBodyBuildCoordinator.cpp` (inicializador de campo) y
  `ConstructorCandidateEvaluation.cpp`: rechazo de `T**` hacia `T const**`.
- `OperatorAndConditionalExpressions.cpp`: condicional y `??` exigen la misma calificación en punteros a puntero.

NSMeta no cambia de formato. Transporta el mismo calificador por parámetro y retorno, y productor y consumidor lo
leen igual.

## Pruebas

**Nueva:** `pruebas-tanda-08-punteros-c/validar-puntero-a-puntero-const.py`. Aprobada en O0–O3, directo y desde
una plantilla precompilada consumida sin el encabezado, con los rechazos en O0.

- **Control Clang:** C++ rechaza `const int **q = &p` y C avisa «discards qualifiers in nested pointer types».
- **Casos positivos:**
  - lectura por `**q`, `*q` y `q[0]`;
  - `*q = d` y `q[0] = c_limit()`, comprobados en C con `read_const`;
  - una función NativeSharp y un método público del productor (publicado en NSMeta) con parámetro `int const**`;
  - `null`, `var`, el cast explícito y funciones C con `int**`.
- **Rechazos:**
  - inicialización, asignación, llamada, retorno, tres niveles y `var` desde `int**`;
  - `**q = 3`, `int* m = *q` e `int** q = &c`;
  - el condicional mixto;
  - la dirección de un parámetro `in`;
  - un consumidor precompilado que pasa `&p` al `int const**` publicado.

**Actualizada.** Fijaba el diagnóstico anterior; lo autoriza el punto 6 del usuario.

| Prueba | Antes | Ahora |
|---|---|---|
| `global-puntero-const/validar.cmd`: `InvalidFixedAddress` e `InvalidFixedMatrixAddress` (`int** = &c_fixed_slots[0]`, slots `int * const`) | «would discard pointee constness» | «cannot take the address of a readonly pointer» (sería `int* const*`) |

## Regresión

`A:\nsc-interop-build\claude\scripts\regresion-d17.cmd`. Primera ejecución: un fallo, la prueba de la tabla, que se
actualizó y se volvió a ejecutar con resultado 0. Contenido:

- la suite nueva;
- `punteros-c`, `calificadores-c`, `globales-c` (dirección `const` y escalares calificados), `arreglos-c`,
  `incompletos-c` y `record-c/validar-miembros`;
- los validadores `global-puntero-const`, `volatile-punteros-c`, `bool-puntero-c`, `record-c`, `layout` y
  `flexible-c/validar-decay`;
- `pruebas-proyecto/ejecutar-cpp.cmd` y `ejecutar-clase-punteros.cmd`;
- los ejemplos `project_stage27/37/38/39/42/43` con el compilador nuevo;
- `VerifyNativeSharpT42PointersLinkage.cmake` con el compilador nuevo;
- `nsc_cpp_session_tests` y `nsc_project_method_body_model_tests`, recompilados;
- el conjunto de D02.
