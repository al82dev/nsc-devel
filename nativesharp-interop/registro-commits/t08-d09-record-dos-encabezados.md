# T08-D09: struct C equivalente en dos encabezados, usado por valor

Fecha: 2026-11-09. Casillas: T08-D09 y T08-005-03a1 ([decisión anterior](t08-typedef-c.md)).

## Problema

Un proyecto puede recibir el mismo struct (o union) C por dos `CppLibraryReference`:

- dos encabezados con el mismo texto de definición;
- un encabezado común con guardas, incluido por los dos.

C11 (6.2.7) hace compatibles esas definiciones entre unidades de traducción, y Clang les da el mismo layout y el
mismo hash ODR. NativeSharp agrupaba el record como `MultipleDefinitionsPointerCompatible` y solo lo usaba por
puntero:

- `Pair p;` daba «cannot be created by value until object lifetime is implemented»;
- una función C con un `Pair` por valor no se importaba («C/C++ parameter 0 is not yet mapped», «C/C++ return type
  is not yet mapped»).

## Causa raíz

El layout de valor (`objectLayout`) solo se concedía a un grupo con una sola definición, y a los records anónimos de
la misma ubicación física. Lo mismo pasaba con `valueLayoutUsable` en el catálogo NSMeta y con el paso por valor de
`CppSemanticFunction`. Sin embargo, el grupo `MultipleDefinitionsPointerCompatible` ya garantiza la equivalencia,
porque solo se forma con:

- comparación estructural «Equivalent», mismo hash ODR, mismo tamaño y misma alineación;
- miembros reconciliados en `selectCppRecordMemberDefinition`: mismos nombres, offsets, tipos y USR.

## Cambio

- `CppSemanticRecord.cpp`: nueva función `equivalentCRecordsHaveValueLayout`. Concede el layout de valor a un grupo
  `MultipleDefinitionsPointerCompatible` cuando se cumple todo esto:
  - todas sus observaciones son C, struct o union, del mismo tipo;
  - las completas, al menos dos, tienen las dimensiones en bytes de la definición elegida;
  - la alineación es potencia de dos y el tamaño es múltiplo de ella.

  Se usa en dos sitios:
  - `prepareCppRecordObservation`: el tipo importado recibe `objectLayout`, y también `hasCFlexibleArrayMember`
    cuando el struct termina en un arreglo flexible;
  - `captureCppCRecordSnapshot`: `valueLayoutUsable`, para que el consumidor de una biblioteca precompilada tenga el
    mismo layout.
- `CppSemanticFunction.cpp`: un parámetro o retorno por valor admite el grupo `MultipleDefinitionsPointerCompatible`.
  Sigue exigiendo que el tipo importado tenga `objectLayout` con las dimensiones de Clang.

C++ no cambia: un record C++ con varias definiciones conserva la regla anterior.

## Pruebas

**Nueva:** `pruebas-tanda-08-typedef-c/validar-record-dos-encabezados.py`. Aprobada en O0–O3, con rechazos en O0.

- **Formas:** «identico» y «comun». Cada una tiene un control Clang y se ejecuta directa y desde una plantilla
  precompilada consumida sin los encabezados.
- **Casos positivos:**
  - locales `Pair` y `Num` (union), y copias;
  - argumentos y retornos por valor a funciones de las dos bibliotecas, y paso por puntero;
  - la global C `g_pair`, leída y escrita;
  - `Outer`, con un campo `Pair`, pasado por valor;
  - `sizeof` y `alignof`.
- **Struct con arreglo flexible duplicado:** el local tiene la parte fija (`sizeof(Flex) == 4`), igual que con una
  sola definición.
- **Rechazos:** si las dos definiciones difieren en el tipo de un miembro, en su nombre o en su orden, se mantiene
  «referenced C/C++ record has conflicting or unverified definitions».

**Actualizada:** `tests/unit/CppClangSessionTests.cpp`. Lo autoriza el punto 6 del usuario.

- «repeated C record observations do not claim value layout» pasa a «repeated equivalent C record observations
  claim value layout».
- Nuevo caso: si una observación tiene otras dimensiones, no hay layout de valor.
- La prueba del códec («C record snapshot and codec preserve a consistent volatile model») espera ahora que
  `valueLayoutUsable` se conserve en `true`.

## Regresión

`A:\nsc-interop-build\claude\scripts\regresion-d09.cmd`, resultado 0:

- la suite nueva;
- `typedef-c`: `validar-reconciliacion` y `validar-formas`;
- `record-c`: `validar-miembros` y `validar.cmd`;
- `globales-c\validar-enum-record`;
- `identidad-c`: `records-precompiled` y `anonimos-anidados`;
- `anonimos-c`, `flexible-c` y `union`;
- `nsc_cpp_session_tests`;
- el conjunto de D02.
