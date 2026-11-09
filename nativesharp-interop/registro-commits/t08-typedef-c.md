# T08-005: typedef C

Fecha: 2026-11-08. Casillas: T08-005-02a–e y T08-005-03a–c.

## Defectos corregidos

### 1. Cast a un typedef C en una plantilla pública (03c)

- **Síntoma.** Una plantilla pública con `(Color)2` o `(MyInt)2` impedía publicar la biblioteca: «generated
  precompiled public template surface is not valid NativeSharp source: … expected ';', found '2'».
- **Causa.** La relectura de la superficie de plantillas
  (`PrecompiledSourceGeneration.cpp`, `verifyPrecompiledGenericTemplateSourceText`) analizaba el texto
  solo con los nombres de tipo de la metadata NativeSharp. El analizador sintáctico decide si
  `(Nombre)expr` es un cast consultando los nombres de tipo conocidos, y los typedef, records y enums C del
  catálogo no estaban entre ellos.
- **Corrección.** La relectura decodifica el catálogo C de la metadata y añade como nombres de tipo
  conocidos los alias de dependencia y los nombres de records y enums C. El producto sintáctico publicado
  es el que reutiliza el consumidor, así que el consumidor sin encabezado también lo recibe.

### 2. El mismo typedef escalar en el consumidor y en el catálogo (03c)

- **Síntoma.** Un consumidor que incluye los mismos encabezados que el productor y usa `score_t` se
  rechazaba con «conflicting C/C++ type alias 'score_t' (projects 1,0; types 'int','int'; USRs
  'c:b.h@T@score_t','c:a.h@T@score_t')».
- **Causa.** Entre proyectos distintos solo se aceptaban los alias de record, enum o puntero a función
  reconciliados. Un typedef escalar idéntico quedaba en conflicto en cuanto los USR diferían en el nombre
  del archivo. Dentro de un proyecto, ese caso ya se aceptaba.
- **Corrección.** Entre proyectos se aceptan dos typedef C de ámbito de archivo que cumplen todo esto:
  - tienen el mismo nombre;
  - tienen el mismo tipo canónico y el mismo destino;
  - su tipo no es record, enum ni puntero a función;
  - sus USR son iguales o solo difieren en el archivo (forma `c:<archivo>@T@<nombre>`).

  Un tipo canónico distinto sigue siendo conflicto.

## Pruebas

Nuevas en `pruebas-tanda-08-typedef-c/`. Ambas aprobadas: casos positivos en O0–O3, rechazos en O0.

- `validar-formas.py` (02a–e). Cada sección se ejecuta en tres caminos: control Clang, directo con el
  encabezado y plantillas de un productor precompilado consumidas sin encabezado.
  - **02a.** Cadenas `i8_t → small_t`, `u16_t → word_t`, `i64_t → big_t` y `u32_t → mask_t`. Se
    comprueban casts, negación de `-128`, vuelta de `65535` a 0, producto de 64 bits y complemento.
  - **02b.** `const int*`, `volatile int*` y `const volatile int*` como parámetro, retorno y local.
  - **02c.** `typedef int vec3[3]` como global, local, campo y parámetro que decae. Puntero a función
    `binop` como local, campo, argumento y retorno.
  - **02d.** `struct` y `union` con y sin etiqueta, y un struct con una union anónima como miembro.
  - **02e.** Enum con nombre y enum anónimo, incluidas las conversiones que C también admite (`Color c =
    4`, `Level l = Red`).
  - **Rechazos.** NativeSharp es más estricto que C, que solo avisa, al quitar `const` o `volatile` del
    apuntado. Clang también rechaza escribir a través de `const`, asignar un puntero a función de otra
    firma y asignar un record a otro.
- `validar-reconciliacion.py` (03a–c).
  - **03a.** `a.h` declara `typedef int score_t` y `b.h` lo declara mediante la cadena
    `base_t → score_t`. Se usan directo, en el productor y en un consumidor sin encabezados. El control
    Clang compila ambos en una unidad.
  - **03b.** `x.h` e `y.h` declaran `score_t` (`int` / `long long`) y `Box` (dos structs anónimos):
    - el proyecto compila mientras no usa los nombres;
    - local, cast y `Box` se rechazan;
    - dos productores, uno por encabezado, publican plantillas con esos nombres. Un consumidor sin
      encabezados que usa las dos se rechaza con «conflicting C/C++ type alias 'score_t'» (contrato vigente,
      decisión T08-005-03b1).

    El control Clang rechaza las redefiniciones.
  - **03c.** Un consumidor sin encabezados llama a plantillas y a métodos públicos no genéricos con
    `score_t` y `Pair` en la firma. Un consumidor con los encabezados mezcla sus valores con los del
    productor. Un consumidor con otro `score_t` sigue en conflicto.

Regresión: `regresion-typedef.cmd` (todo aprobado), que incluye:

- `typedefs`, `typedefs/precompiled-conflict`, `typedefs/precompiled-enum` y `typedefs/EnumConflict`;
- `nombres-metadata/validar-referenciados.cmd`, `meta-c`, `incompleto/validar-opaco.cmd` y
  `typedef-funcion-record`;
- las plantillas enum y alias, y las pruebas de typedef calificados.

También pasaron el validador de identidad `pruebas-tanda-08-identidad-c/validar.cmd` (desde la copia CRLF) y
`regresion-callbacks.cmd`, salvo el fallo conocido T08-017-02a.

## Decisiones pendientes

- **T08-005-03b1: typedef C homónimos e incompatibles de dos productores en un mismo consumidor.**
  - Hoy, un consumidor que usa las plantillas de dos productores, cada uno con su `score_t` (`int` /
    `long long`) o su `Callback` sobre records distintos, se rechaza con «conflicting C/C++ type alias».
    La causa es que los módulos precompilados ven todos los demás módulos de la compilación, y
    `expandCppTypeAlias` reúne los alias de ambos catálogos.
  - La prueba existente `typedef-funcion-record/validar.py --incompatible` (y `--same-size-incompatible`,
    `--const-mismatch`) fija ese rechazo.
  - La alternativa es que cada plantilla resuelva el typedef en el catálogo de su productor. Es lo que ya
    hacen las constantes enum C (`t08-enum-productor-codigo-consumidor.md`) y las globales `static`
    homónimas (`t08-globales-consumo.md`).
  - Se implementó y funcionaba con un marcador `fromPrecompiledCatalog` en `CppImportedTypeAlias`, pero
    contradice esa prueba existente. Se revirtió.
- **T08-005-03a1: objetos por valor de un struct C definido en dos encabezados.** El mismo struct puede
  llegar al proyecto por dos `CppLibraryReference`:
  - definido en dos encabezados con texto idéntico;
  - definido en un encabezado común con guardas, incluido por ambos.

  En los dos casos el struct se agrupa como `MultipleDefinitionsPointerCompatible`. Se usa por puntero y
  sus campos son accesibles, pero `Pair p;` se rechaza con «C/C++ object local 'p' cannot be created by
  value until object lifetime is implemented».

  Solo el caso con miembros anónimos de la misma ubicación física recibe dimensiones de valor. La prueba
  unitaria existente «repeated C record observations do not claim value layout» fija que las
  observaciones repetidas no reclamen layout de valor. Admitir objetos por valor cambiaría ese contrato.
  La prueba de 03a usa por eso el struct desde un solo encabezado. Relacionado con T08-010-07.
