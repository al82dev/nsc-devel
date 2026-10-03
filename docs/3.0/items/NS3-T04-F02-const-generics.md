# NS3-T04-F02 — Const generics

## Estado

`TERMINADA`. NativeSharp 3 materializa parámetros const enteros, booleanos, enum y tuplas
estructurales aprobadas con evaluación determinista, identidad normalizada e inferencia exacta.

## Qué se hizo

- El parser conserva expresiones const ordinarias dentro de argumentos genéricos y desambigua sus
  paréntesis frente a `<`/`>` de tipos. Operadores a nivel superior no reabren la antigua
  confusión entre una comparación y una llamada genérica.
- `TypeSyntax` transporta temporalmente el AST de la expresión; el modelo genérico la evalúa y la
  sustituye por el marcador canónico antes de crear la especialización.
- Se admiten enteros con signo, booleanos, enums nominales y tuplas estructurales recursivas. Los
  enumeradores explícitos pueden depender de miembros anteriores del mismo enum.
- La normalización comprueba rango y signedness con `ProjectIntegerEvaluator`. Operaciones
  indefinidas o runtime fallan cerradas.
- La identidad de enum incluye módulo y declaración. La identidad de tupla usa elementos const
  codificados con longitud; su sustitución reconstruye un `TupleExpression` real dentro del cuerpo.
- La inferencia distingue parámetros de tipo y parámetros const. Puede unificar valores const en
  tipos genéricos, tipo esperado exacto y dimensiones de arreglo fijo, y rechaza evidencia
  contradictoria.
- El mangling físico continúa usando el hash estable de Stage 44 y sólo caracteres válidos para
  símbolos Win64. Valores equivalentes producen un único cache entry.
- Layouts `Buffer<T,N>` y matrices con dos dimensiones pasan por el pipeline ordinario; no se añadió
  representación runtime nueva.

## Archivos modificados

- `include/nativesharp/AST/TypeSyntax.h`, `src/AST/TypeSyntax.cpp`: carrier temporal de expresión.
- `src/Parser/Parser.cpp`: gramática de expresiones const y lookahead no ambiguo.
- `include/nativesharp/Frontend/ProjectGenericModel.h`,
  `src/Frontend/ProjectGenericModel.cpp`: evaluación, enums, tuplas, normalización, sustitución e
  inferencia.
- `CMakeLists.txt`: target dirigido de F02.
- `docs/3.0/TANDA-04-GENERICOS-3.0.md` y
  `docs/3.0/TANDA-OPERATIVA-11-T04-CONST-GENERICS-Y-PACKS.md`: estado consolidado sin deuda F02
  obsoleta.

## Archivos nuevos y por qué

- `tests/unit/NativeSharp3T04ConstGenericTests.cpp`: prueba productiva aislada de expresiones,
  vectores, matrices, signo, enums, tuplas, inferencia, overflow, determinismo, caché, mangling y
  regresión de desambiguación.
- `docs/3.0/language/const-generics.md`: contrato bilingüe de usuario.
- Este documento: evidencia atómica del commit y de su validación.

## Mejora obtenida

El tamaño, la estrategia o una clave estructural pueden expresarse como parte del tipo sin estado
runtime. El compilador genera exactamente una especialización para valores equivalentes, conserva
la nominalidad de enums, detecta overflow antes del backend e informa de forma estable cuando una
expresión no es constante.

## Validación dirigida

- `nsc_ns3_t04_const_generic_tests`: 16/16, incluidos PE O0 enlazados con el NSLink local.
- `nsc_ns3_t04_declarative_constraint_tests`: regresión F01 dirigida.
- `nsc_ns3_t04_generic_tests`: presupuestos de profundidad/cantidad y contrato genérico común.
- `c61_ns3_const_generics_packs`: fixture O0/NSLink existente para layout y compatibilidad con F03.

No se ejecutó baseline, CTest general, O1/O2/O3, instalación ni deploy.

