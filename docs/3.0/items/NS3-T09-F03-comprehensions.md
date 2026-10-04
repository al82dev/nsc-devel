# NS3-T09-F03 — Comprehensions with explicit builders

## Español

### Resultado

NativeSharp 3 incorpora comprensiones mediante una forma explícita y predecible:

```ns
SumBuilder result = comprehend(seed) {
    foreach (int value in 0..10) {
        if ((value % 2) == 0) yield return value;
    }
};
```

La expresión inicial es el builder. Se evalúa exactamente una vez, se conserva en almacenamiento
local y cada `yield return value;` —o la abreviatura `yield value;`— invoca su método ordinario
`Add(value)`. `yield break;` termina la comprensión y devuelve el estado acumulado.

### Decisiones de diseño

- No existe una colección implícita ni una asignación oculta en heap: el usuario elige el builder.
- La resolución de `Add` usa la búsqueda normal de miembros y sobrecargas; un builder inválido recibe
  el diagnóstico normal del lenguaje.
- Se admiten builders copiables y move-only. Para entregar un builder move-only se escribe
  `comprehend(move(builder))`; el resultado se transfiere sin copia.
- Los generadores, filtros y anidamientos son sentencias ordinarias dentro del bloque, por lo que
  conservan las reglas existentes de `foreach`, `if`, ownership, `noheap` y limpieza.
- La característica requiere `<Language>3</Language>`.

### Implementación

El parser desazucara la comprensión a una expresión `loop` tipada: declara un builder oculto,
convierte cada yield en `builder.Add(...)` y finaliza con un break con valor. Esto reutiliza el
frontend, HIR, MIR y backend de control de flujo ya existentes. El movimiento sintético del
resultado tiene una marca exclusiva que permite copiar tipos `copy` y consumir tipos move-only sin
relajar la validación de `move(...)` escrita por el usuario.

### Archivos

- `include/nativesharp/Parser/Parser.h`: contexto y entradas privadas del parser.
- `src/Parser/Parser.cpp`: reconocimiento, desazucarado y yields contextuales.
- `src/Frontend/ProjectMethodBodyModel.cpp`: transferencia interna, limitada al resultado de una
  comprensión.
- `tests/unit/NativeSharp3T09ComprehensionTests.cpp`: prueba dirigida O0/NSLink.
- `CMakeLists.txt`: objetivo aislado `nsc_ns3_t09_comprehension_tests`.

### Validación puntual

El ejecutable dirigido completó 7/7 comprobaciones con O0 y NSLink. No se ejecutó baseline ni suite
general.

## English

### Result

NativeSharp 3 now provides comprehensions through an explicit, predictable builder form. The seed
expression is evaluated exactly once, each `yield return value;` (or `yield value;`) performs normal
`Add(value)` member lookup, and `yield break;` returns the accumulated builder immediately.

The compiler does not select or allocate a hidden collection. Copyable builders are copied according
to their declared semantics; move-only builders enter through `comprehend(move(builder))` and leave
the expression through a compiler-generated ownership transfer. Nested generators and filters reuse
ordinary statements and therefore retain existing `foreach`, ownership, `noheap`, and cleanup rules.

The parser lowers the feature to an already-supported typed loop expression, so no parallel HIR/MIR
or backend control-flow path was added. The dedicated O0/NSLink executable passed all 7 checks; no
baseline or general suite was run.
