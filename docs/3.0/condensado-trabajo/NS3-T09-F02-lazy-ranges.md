# NS3-T09-F02 — Rangos perezosos / Lazy ranges

## Español

NativeSharp 3.0 representa un rango entero como tres valores escalares: inicio, límite y salto.
`foreach` enlaza esos valores una sola vez, de izquierda a derecha, y genera directamente el
bucle; no crea arrays, listas ni enumeradores de heap.

Las formas admitidas son:

```nativesharp
0..10                 // límite exclusivo, step +1
0..=10                // límite inclusivo, step +1
10..0 step -2         // descendente, límite exclusivo
1..=9 step 2          // inclusivo con salto explícito
```

`step` es contextual y sólo tiene significado después de un delimitador de rango. Un salto cero,
un salto constante que se aleja del límite y un conteo constante que no cabe en `size_t` producen
diagnóstico de compilación. Si el salto se calcula en runtime, el lowering inserta una aserción
antes de entrar al bucle para impedir que cero o una dirección inválida se conviertan
silenciosamente en una secuencia vacía. El incremento usa la aritmética entera ordinaria del
lenguaje, por lo que su política de overflow sigue siendo observable.

Los endpoints y el salto aceptan tipos enteros. Los rangos funcionan dentro de funciones
`noheap`; su costo de almacenamiento es el de los escalares enlazados y el cursor del bucle. La
fusión con adaptadores conserva un contrato separado en `NS3-T09-F06`.

## English

NativeSharp 3.0 represents an integer range as three scalar values: begin, bound, and step. A
`foreach` binds them exactly once from left to right and emits the loop directly; it creates no
array, list, or heap enumerator.

Supported forms are:

```nativesharp
0..10                 // exclusive bound, step +1
0..=10                // inclusive bound, step +1
10..0 step -2         // descending, exclusive bound
1..=9 step 2          // inclusive with an explicit step
```

`step` is contextual and is meaningful only after a range delimiter. A zero step, a constant step
that moves away from the bound, and a constant element count that does not fit in `size_t` are
compile-time diagnostics. When the step is computed at runtime, lowering inserts an assertion
before the loop so zero or an invalid direction cannot silently become an empty sequence. The
increment uses ordinary language integer arithmetic, so its overflow policy remains observable.

Endpoints and step accept integer types. Ranges are valid inside `noheap` functions; their storage
cost is the bound scalars and loop cursor. Adapter fusion has its own contract in `NS3-T09-F06`.

## Evidencia dirigida / Directed evidence

- `nsc_ns3_t09_lazy_range_tests`
- 6 checks: ejecución O0/NSLink, inclusividad, descenso, constantes, `long`, `noheap`, orden de
  evaluación, salto cero, dirección inválida, overflow de conteo y ausencia de materialización.
- No se ejecutó baseline ni suite general.

## Archivos / Files

- `include/nativesharp/Lexer/TokenKind.h`
- `src/Lexer/Lexer.cpp`
- `src/Lexer/TokenKind.cpp`
- `src/Parser/Parser.cpp`
- `src/Frontend/ProjectMethodBodyModel.cpp`
- `src/Language/IterationSemantics.cpp`
- `tests/unit/NativeSharp3T09LazyRangeTests.cpp`
- `CMakeLists.txt`
