# 281 — Modificador `forceinline` / `forceinline` modifier

## Resumen / Summary

`forceinline` ya es una política obligatoria de métodos concretos. Se conserva desde el lexer hasta
MIR, se baja a `alwaysinline` y ejecuta la expansión obligatoria en O0 sin habilitar el inlining
automático. Los contratos imposibles se diagnostican en fuente.

`forceinline` is now a required policy for concrete methods. It is preserved from the lexer through
MIR, lowers to `alwaysinline`, and performs mandatory expansion at O0 without enabling automatic
inlining. Impossible contracts are diagnosed at the source level.

## Archivos y funciones / Files and functions

- `include/nativesharp/Lexer/TokenKind.h`, `src/Lexer/Lexer.cpp` y `src/Lexer/TokenKind.cpp` —
  reservan y nombran `forceinline`.
- `include/nativesharp/Parser/Parser.h` y `src/Parser/Parser.cpp` — aceptan el modificador sólo en
  métodos ordinarios con cuerpo y rechazan duplicados, abstracción y `inline forceinline`.
- `src/Frontend/ProjectMemberModel.cpp` — selecciona `InliningPolicy::Required`.
- `src/Frontend/ProjectMethodBodyModel.cpp` — forma el grafo de llamadas obligatorias y rechaza
  recursión directa o ciclos íntegramente `forceinline`.
- `src/LLVM/ProjectOopLlvmLowerer.cpp` — agrega `alwaysinline` a definiciones obligatorias.
- `tests/unit/ForceInlineModifierTests.cpp` — comprueba política, atributo, expansión O0 y errores.
- `docs/1.0.1/` — actualiza gramática, especificación, contrato, mapas, libro, estado y roadmap.

## Implementación y decisiones / Implementation and decisions

La obligación pertenece al destino y usa la misma `InliningPolicy` exclusiva creada en el ítem 278.
No se añadió un booleano paralelo a HIR/MIR. `alwaysinline` conserva la definición; sólo ordena que
los call sites legales se expandan. El pipeline O0 ya ejecuta el paso obligatorio sin activar las
heurísticas automáticas formalizadas en el ítem 279.

The obligation belongs to the callee and uses the exclusive `InliningPolicy` introduced by item
278. No parallel HIR/MIR boolean was added. `alwaysinline` preserves the definition and only
requires legal call sites to expand. The O0 pipeline already runs the mandatory pass without
enabling the automatic heuristics formalized by item 279.

La validación de recursión conserva ciclos legítimos con un límite ordinario. Sólo se crea una
arista cuando caller y callee son obligatorios; por ello se rechaza `A -> A` y
`forceinline A -> forceinline B -> A`, pero no `forceinline A -> normal B -> A`. En el último caso,
la llamada al método obligatorio sí puede expandirse y la llamada normal termina el crecimiento.

Recursion validation preserves legitimate cycles with an ordinary boundary. An edge is created only
when both caller and callee are required; therefore `A -> A` and
`forceinline A -> forceinline B -> A` are rejected, while `forceinline A -> normal B -> A` is not.
In the latter case the required call can expand and the ordinary call stops further growth.

## Evidencia dirigida / Focused evidence

Un helper aritmético marcado `forceinline` atraviesa OOP MIR, presenta política `Required`, emite
`alwaysinline` y pierde su instrucción `call` después de optimizar en O0. Casos abstractos,
contradictorios, recursivos directos y recursivos mutuos se rechazan con diagnóstico preciso.

An arithmetic helper marked `forceinline` crosses OOP MIR, carries the `Required` policy, emits
`alwaysinline`, and loses its `call` instruction after O0 optimization. Abstract, contradictory,
directly recursive, and mutually recursive cases are rejected with precise diagnostics.

No se ejecutó baseline para esta entrega. / No baseline was run for this delivery.

## Commit de referencia / Reference commit

**Título / Title**

`Añade forceinline obligatorio / Add required forceinline`

**Descripción / Description**

`Propaga forceinline como política obligatoria, emite alwaysinline, fuerza expansión incluso en O0 y rechaza contratos abstractos, combinaciones contradictorias y ciclos recursivos imposibles. / Carries forceinline as a required policy, emits alwaysinline, forces expansion even at O0, and rejects abstract contracts, contradictory combinations, and impossible recursive cycles.`
