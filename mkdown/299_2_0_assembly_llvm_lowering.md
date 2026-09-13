# 299 — Lowering LLVM de asm / Assembly LLVM lowering

## Resultado / Result

Los bloques integrados producen inline assembly LLVM verificable y un objeto COFF x64 real. La
emisión cubre todas las clases de operandos y efectos cerradas en los items 289–297. / Integrated
blocks now produce verifier-clean LLVM inline assembly and a real x64 COFF object. Emission covers
every operand and effect class established by items 289–297.

## Archivos y funciones / Files and functions

- `src/LLVM/ProjectOopLlvmLowerer.cpp`
  - `lowerInlineAssembly()` ordena el contrato MIR, baja operands, construye constraints, reescribe
    placeholders, emite la llamada y almacena resultados.
  - `lowerStatement()` despacha el nodo asm como sentencia con efectos.
- `src/LLVM/LlvmCApi.h`
  - Declara `LLVMGetInlineAsm`, dialectos y el constructor de struct literal requerido por outputs
    múltiples.
- `src/LLVM/LlvmTargetMachine.cpp`
  - Inicializa el parser x86 además del printer del target.
- `include/nativesharp/Parser/Parser.h`, `src/Parser/Parser.cpp` y
  `include/nativesharp/Diagnostics/DiagnosticId.h`
  - Validan `$N`, `$outN` y `$$` antes de HIR mediante `InvalidInlineAssemblyPlaceholder`.
- `tests/unit/IntegratedAssemblyGrammarTests.cpp` y `IntegratedAssemblyMirTests.cpp`
  - Certifican diagnósticos, IR, verifier y emisión COFF en O0.
- `docs/1.0.1`
  - Actualiza especificación, contrato, trazabilidad, estado, changelog y roadmap.

## Implementación y decisiones / Implementation and design decisions

El dialecto fuente es Intel. El espacio LLVM enumera outputs antes que inputs, por lo que la
reescritura conserva los dos espacios visibles de NativeSharp sin exigirle al usuario conocer esa
convención. Outputs directos vuelven como scalar/struct; memoria usa `=*m` y, para lectura-escritura,
un input `*m` emparejado, siempre con atributo
`elementtype`. / Source uses Intel dialect. LLVM numbers outputs before inputs, so rewriting keeps
NativeSharp's two public spaces without exposing that convention. Direct outputs return as a
scalar/struct; memory uses `=*m` plus a paired `*m` input for read-write, with `elementtype`.

Los labels se reescriben con `${:uid}` porque un método puede inlinarse o un bloque repetirse; un
nombre assembler fijo podría colisionar después de optimizar aunque fuera único en su AST original.
/ Labels use `${:uid}` because a method may be inlined or a block repeated; a fixed assembler name
could collide after optimization even when unique in its original AST.

## Evidencia enfocada / Focused evidence

```text
nsc.integrated-assembly.grammar: PASS: 49 checks
nsc.integrated-assembly.mir: PASS: 17 checks
LLVM verifier: passed
Windows x64 O0 COFF emission: passed
```

No se ejecutó baseline. / No baseline was run.

## Mensaje de commit / Commit message

**Título / Title**

`Emite asm integrado mediante LLVM x64 / Emit integrated assembly through x64 LLVM`

**Descripción / Description**

`Baja operands, constraints, outputs, clobbers y labels locales a inline asm Intel verificable y COFF. / Lower operands, constraints, outputs, clobbers, and local labels to verifier-clean Intel inline asm and COFF.`
