# Tanda operativa 31 — Borrow de arrays, relaciones no propietarias y jagged arrays

Fecha: 2026-09-30

Rama: `development`

Estado: implementación terminada y validación dirigida O0 correcta; baseline no ejecutado y
reservado para la ejecución externa de Alberto

## Objetivo

Cerrar como una sola vertical verificable los huecos de arrays y préstamos que compartían la
misma frontera de ownership:

1. convertir un `T[]` o un `T[N]` en una vista prestada sin copia ni allocation oculto;
2. impedir que una vista escape del owner local o que el owner se mueva mientras la vista vive;
3. construir, indexar, publicar por NSMETA y destruir arrays escalonados `T[][]`;
4. conservar `new T[] { ... }`, ampliarlo a elementos owning y permitir el equivalente jagged
   `new T[][] { ... }`;
5. expresar una relación no propietaria con un tipo de biblioteca que nunca destruye el destino;
6. publicar en HIR/MIR la representación y los costos físicos reales.

T31 no introduce arrays rectangulares `T[,]`: rango, strides y layout multidimensional pertenecen
exclusivamente a T32.

## Sintaxis y tipo de jagged arrays

El parser acepta y canonicaliza las dos formas unidimensionales equivalentes:

```nativesharp
int[][] a = new int[rows][];
int[][] b = new int[][rows];
```

Ambas producen un array exterior con longitud `rows` y elementos de tipo `int[]`. Solo puede
existir una dimensión contada en esta construcción. Dos dimensiones contadas reciben un
diagnóstico que dirige el caso hacia los arrays de rango de T32, en vez de fingir que son jagged.

El inicializador inferido conserva evaluación y conversiones por elemento:

```nativesharp
int[] flat = new int[] { 20, 22 };
int[][] nested = new int[][] { new int[] { 42 } };
int[] empty = new int[] { };
```

En la forma jagged, uno de los `[]` representa el array exterior cuya longitud se infiere y los
anteriores forman el tipo de elemento. No se crea una representación paralela: el tipo canónico
sigue siendo `builtin:int[][]`.

## Ownership y destrucción

Antes de T31, `delete[]` liberaba solamente el bloque exterior. Un `T[][]` perdía los rows todavía
almacenados y un array de clases, strings o delegates perdía sus elementos owning.

La nueva ruta de LLVM aplica estas reglas:

- un array nativo se clasifica como un valor que requiere destrucción owning;
- si su elemento es ownable —otro array, string, delegate o clase/interface— se recorren sus
  slots;
- cada slot se pone en `null` **antes** de ejecutar destrucción de usuario;
- los arrays anidados se destruyen recursivamente;
- el bloque exterior se libera al final;
- un elemento previamente eliminado mediante `delete[] rows[i]` queda nulo, por lo que eliminar
  después `rows` no lo libera dos veces;
- arrays de valores triviales siguen liberando un solo bloque y no pagan el loop de elementos.

La misma clasificación alimenta `delete[]`, cleanup de local RAII y cleanup de parámetro owned,
evitando que las tres rutas diverjan.

## Bridges prestados

El bridge canónico dinámico ya existente queda ejercitado productivamente:

```nativesharp
Span<int> mutable = Spans.Of<int>(items);
ReadOnlySpan<int> readOnly = mutable.AsReadOnly();
```

Para un array inline, `Length` ahora es una constante derivada del tipo y el bridge no copia:

```nativesharp
int[4] values;
Span<int> view = Spans.OfRaw<int>(&values[0], values.Length);
```

`T[N].Length` no lee un header inexistente: el MethodBody lo reconoce como
`FixedArrayLength` y HIR lo materializa como `integer-constant`. El array fijo continúa inline.

Las reglas de `borrowed struct` ya rechazaban retorno desde owner local, almacenamiento en field,
captura y mutabilidad no concedida. T31 cierra el hueco restante: las vistas registradas mediante
`viewRootLocalName` ahora también cuentan como borrows activos para `move(owner)`.

## Relación no propietaria

`NativeSharp.Core.BorrowedReference<T>` es un `borrowed struct` de un solo field. No tiene
destructor, no ejecuta `delete` y no aloja memoria. Por ser view type:

- no puede guardarse en un field ordinario o estático;
- no puede capturarse en un lambda;
- no puede devolverse cuando está enraizado en un owner local;
- bloquea el movimiento de ese owner durante su scope;
- desaparece al salir del scope sin destruir el objeto observado.

Esto permite expresar relaciones temporales no owning sin relajar la prohibición correcta de
fields `borrowed` sin lifetime demostrable. Una referencia débil persistente con control block es
otro contrato y no se fingió en esta tanda.

## Cruce de módulo y visibilidad

Una biblioteca productora publica `int[][]` en NSMETA como `builtin:int[][]`. Un consumidor que
solo tiene `.lib` + `.nsmeta` puede pasar el valor, indexarlo y enlazarlo en O0. HIR conserva
`array-allocation` y el tipo anidado; MIR conserva `array-allocate`, `deallocate` y la misma clave.

No aparece una copia de rows, un wrapper heap ni un layout oculto. Cada nivel jagged es el array
nativo existente: header de longitud y slots pointer-sized en Windows x64.

## Archivos modificados

- `src/Parser/Parser.cpp`: secuencia de brackets de `new`, una dimensión contada, modifiers de
  elemento y forma inferida jagged.
- `include/nativesharp/Frontend/ProjectMethodBodyModel.h`: operación core
  `FixedArrayLength`.
- `src/Frontend/ProjectMethodBodyModel.cpp`: reconocimiento y constante de `T[N].Length`, nombre
  auditable de la operación y bloqueo de `move(owner)` por view locals activos.
- `src/Frontend/ProjectOopHirModel.cpp`: plegado HIR de `T[N].Length`; `delete[]` de elemento
  baja la dirección del slot para limpiarlo antes del drop.
- `src/LLVM/ProjectOopLlvmLowerer.cpp`: destrucción de elementos owning, recursión jagged,
  null-before-drop y clasificación unificada de arrays owning.
- `CMakeLists.txt`: registro de la prueba dirigida T31.

## Archivos nuevos y por qué

- `lib/NativeSharp.Core/BorrowedReference.ns`: relación no propietaria canónica, de un pointer,
  sin heap y sometida a las reglas de escape de view types.
- `tests/unit/NativeSharp3T31ArrayBorrowTests.cpp`: vertical O0 autocontenida que genera productor
  `.lib/.nsmeta`, compila el consumidor, ejecuta destrucción real y comprueba diagnósticos de
  escape, retención, move del owner y mutabilidad.
- `docs/3.0/TANDA-OPERATIVA-31-ARRAY-BORROW-JAGGED.md`: expediente de contrato, representación,
  archivos, evidencia y límites de este commit.

## Validación dirigida ejecutada

1. `nsc` construido desde `A:\nsc-devel\build-lsp-debug`, Debug/O0: correcto.
2. `nsc_ns3_t31_array_borrow_tests`: `PASS: 12 checks`, incluido objeto `-g -O0` con ambos
   nombres de header jagged en CodeView.
3. `nsc_parser_tests`: `PASS: 23 checks`.
4. `nsc_stage41_arrays_strings_core_tests`: `PASS: 76 checks`.
5. `nsc_stage58_view_lifetimes_tests`: `PASS: 31 checks`.
6. `git diff --check`: correcto antes del commit.

La prueba histórica `nsc_stage50_11_ownership_execution_tests` no se usa como evidencia de T31:
se invocó una vez y se comprobó que internamente ejecuta también O2 —fuera de la metodología— y
que la mayoría de sus fixtures consultan un runtime anterior sin el símbolo ABI 1.1 de stdin.
Falló antes de validar esta implementación y no se repitió. La destrucción owning relevante sí se
ejercitó de extremo a extremo en la prueba T31, exclusivamente en O0.

No se ejecutó baseline, CTest general, compilación de libraries, instalación, deploy, componentes
externos ni operaciones remotas. El baseline integral sigue reservado para Alberto.

## Mejora concreta

NativeSharp puede representar tablas ragged, listas de buffers y colecciones de filas sin perder
ownership. El mismo valor cruza fuente/precompilado, cada owner se destruye exactamente una vez y
las vistas de arrays no copian ni permiten mover su almacenamiento mientras siguen activas.

## Límites deliberados

- `T[,]`, dimensiones múltiples contadas, strides y rectangular layout pertenecen a T32.
- `BorrowedReference<T>` es una relación lexical no owning, no una weak reference persistente con
  conteo o control block.
- `Span<T>` conserva su contrato raw para `OfRaw`: el caller afirma que puntero y longitud son
  compatibles.
- La excepción durante una secuencia de inicialización owning no crea en T31 una transacción de
  construcción nueva; la política completa de inicialización parcial multidimensional se cierra
  en T32.
- No se cambia el ABI externo: se reutilizan el layout y las claves de tipo de arrays nativos.
- El baseline no forma parte de esta tanda.
