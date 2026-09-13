# 282 — Modificador `noinline` / `noinline` modifier

## Resumen / Summary

`noinline` ya prohíbe la expansión de un método concreto en todos los niveles de optimización. La
política atraviesa frontend, HIR y MIR, y se materializa como el atributo LLVM `noinline`.

`noinline` now prevents expansion of a concrete method at every optimization level. The policy
crosses the frontend, HIR, and MIR and materializes as LLVM's `noinline` attribute.

## Archivos y funciones / Files and functions

- `include/nativesharp/Lexer/TokenKind.h`, `src/Lexer/Lexer.cpp` y `src/Lexer/TokenKind.cpp` —
  reservan y nombran la palabra clave.
- `include/nativesharp/Parser/Parser.h` y `src/Parser/Parser.cpp` — validan ubicación, duplicados y
  exclusión mutua con `inline` y `forceinline`.
- `src/Frontend/ProjectMemberModel.cpp` — selecciona `InliningPolicy::Forbidden`.
- `src/LLVM/ProjectOopLlvmLowerer.cpp` — emite el atributo `noinline` para definiciones nativas.
- `tests/unit/NoInlineModifierTests.cpp` — comprueba transporte, IR O2 y diagnósticos negativos.
- `docs/1.0.1/` — actualiza gramática, especificación, contrato, mapas, libro, estado y roadmap.

## Implementación y decisiones / Implementation and decisions

La prohibición usa la misma política exclusiva que `inline` y `forceinline`; no se agregó un flag
backend independiente. El atributo se coloca sobre la definición, por lo que todos sus call sites
reciben la misma garantía y el modelo de costo no puede contradecirla en O1, O2 u O3.

The prohibition uses the same exclusive policy as `inline` and `forceinline`; no independent backend
flag was added. The attribute belongs to the definition, so every call site receives the same
guarantee and the cost model cannot override it at O1, O2, or O3.

`noinline` no es una declaración de linkage. La función conserva exactamente su visibilidad,
convención, exportación e identidad anterior. Imports y contratos abstractos no aceptan el
modificador porque no existe una definición local cuya optimización pueda controlar.

`noinline` is not a linkage declaration. The function retains its previous visibility, calling
convention, export, and identity. Imports and abstract contracts reject the modifier because there
is no local definition whose optimization it could control.

## Evidencia dirigida / Focused evidence

Un helper marcado `noinline` llega a OOP MIR como `Forbidden`, emite `noinline` y conserva su
instrucción `call` después del pipeline O2. Un método abstracto, una combinación con `forceinline` y
un modificador duplicado se rechazan con diagnósticos específicos.

A helper marked `noinline` reaches OOP MIR as `Forbidden`, emits `noinline`, and retains its `call`
instruction after the O2 pipeline. An abstract method, a combination with `forceinline`, and a
duplicate modifier are rejected with specific diagnostics.

No se ejecutó baseline para esta entrega. / No baseline was run for this delivery.

## Commit de referencia / Reference commit

**Título / Title**

`Añade noinline obligatorio / Add enforced noinline`

**Descripción / Description**

`Propaga noinline como política prohibida, conserva llamadas fuera de línea bajo O2 y rechaza ubicaciones, duplicados y combinaciones contradictorias. / Carries noinline as a forbidden policy, preserves out-of-line calls under O2, and rejects invalid placements, duplicates, and contradictory combinations.`
