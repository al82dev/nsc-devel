# T19-002 (avance): `sizeof` de un tipo C como constante y `new` sobre structs y unions C

Fixture nuevo: `pruebas-tanda-19-c-valores/validar.py`, con y sin `--dll`. También cambia
`pruebas-tanda-18-zlib`: `sizeof(z_stream)` pasa a `deflateInit_` sin conversión y `new z_stream()` da el struct en
cero. Sonda de desarrollo: `tools/t19c/d11/sonda-t19-002.py` (`logs/sonda2.log`).

## Defectos

Confirmados con la sonda:

- `take(sizeof(S))` con `int take(int)` daba «no viable overload»: `sizeof` es `nuint` y NativeSharp no lo convierte
  implícitamente a `int`. C lo convierte. zlib necesitaba `(int)sizeof(z_stream)` (T18-009).
- `S s = new S();` sobre un struct C daba «C/C++ object construction requires its object lifetime to be
  implemented»: el análisis rechazaba cualquier construcción de un tipo C/C++ importado. Al quitar ese rechazo para
  los structs y unions C aparecieron tres fallos más:
  - el MIR exigía un constructor seleccionado (`C++ value construction requires a selected constructor`);
  - la planificación de la vida C++ trataba el valor como un objeto C++ (`C++ local lifetime has incomplete cleanup
    or type identity`);
  - LLVM leía el valor como su portador de bytes (`[4 x i8]` para una union de cuatro bytes) y la llamada C
    esperaba `i32` (`Call parameter type does not match function signature!`).

## Decisiones

- `sizeof` y `alignof` de un tipo C/C++ completo (struct, union o record C++ trivial) y de un entero incorporado son
  constantes enteras: Clang fija el layout de los tipos C/C++ antes de analizar el cuerpo y el lenguaje fija el
  ancho de los enteros. Como un literal, la constante se convierte implícitamente a un tipo entero en el que cabe
  (`int`, `unsigned`, `short`, `long long`). Un valor que no cabe se rechaza, como un literal.
- El `sizeof` de un tipo NativeSharp sigue siendo un valor `nuint`: su layout lo decide después el modelo de layout.
- `new S()` sin argumentos sobre un struct o una union C es su valor cero, como `default(S)` y `(S){0}` de C: en un
  local, en una asignación, en un campo y como argumento por valor. Con argumentos se rechaza: un tipo C no tiene
  constructores (`C type 'S' has no constructors; new without arguments gives its zero value`).

## Diseño

- `IndexAndRangeExpressions.cpp`: el `sizeof`/`alignof` de un tipo C/C++ con `objectLayout` (el mismo que usa el
  importador de layouts de MIR) lleva el valor en `cppPhysicalLayoutQueryValue` y en `integerConstant`; el de un
  entero incorporado, solo en `integerConstant`.
- `HirAggregateLowering.cpp`: si el análisis dio una constante, el HIR comprueba que coincide con la del modelo de
  layout (`sizeof constant ... disagrees with the layout model`).
- `ConstructionAndLifetimeExpressions.cpp`: `new S()` de un struct o union C pasa por la construcción implícita sin
  parámetros; con argumentos se rechaza.
- `MirNodeMachine.cpp`: la construcción de valor de un struct o union C sin constructor toma tamaño y alineación del
  layout importado y se inicializa a cero.
- `CppSemanticFunction.cpp`: la planificación de vida C++ salta el valor cero de un struct o union C.
- `OopLlvmConstruction.cpp`: el valor cero de un struct o union C se lee con el tipo valor del tipo, no con su
  portador de bytes.

No se modificó Clang ni NSLink. El contrato NSMeta no cambia.

## Evidencia

Resultados: 508204 en las ocho combinaciones, directo y desde la plantilla precompilada con y sin encabezado,
igual que el control C de Clang 23.1.0, O0–O3.

| Fixture | Estática | DLL |
|---|---|---|
| pruebas-tanda-19-c-valores | `A:/temp/nsc-t19-c-valores-vl7zmpaj` | `A:/temp/nsc-t19-c-valores-jiryjy2f` |
| pruebas-tanda-18-zlib | `A:/temp/nsc-t18-zlib-i75xluhf` | `A:/temp/nsc-t18-zlib-o7xuhd31` |

Rechazo genuino, también en Clang: `sizeof` de un tipo C incompleto (`Opaque`).

Regresión de las tandas 10 a 19 con este cambio: 72 de 72 en estática (`logs/regresion-d36e`) y 53 de 53 en DLL
(`logs/regresion-dll-d36d`; en esa corrida el control C de `pruebas-tanda-19-nulos` O3 tardó 30 s y terminó con 0,
el mismo fallo del entorno visto en T19-007; repetido, pasó entero: `A:/temp/nsc-t19-nulos-03jgutcd`). Los nueve
CTest de interop y la identidad de NSC pasan (`logs/lote-ct2.log`).

## Límites

- La dirección de una global C `_Atomic` sigue en T19-002.
