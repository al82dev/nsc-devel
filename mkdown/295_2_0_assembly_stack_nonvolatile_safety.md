# 295 — Seguridad de stack y registros no volátiles / Stack and nonvolatile-register safety

## Resultado / Result

Los aliases del stack pointer se rechazan como operandos fijos o clobbers. Los registros no
volátiles de Windows x64 se reconocen y marcan para preservación posterior. / Stack-pointer aliases
are rejected as fixed operands or clobbers. Windows x64 nonvolatile registers are recognized and
marked for later preservation.

## Archivos y funciones / Files and functions

- `src/Parser/Parser.cpp`
  - `isInlineAssemblyStackPointer()` bloquea `rsp`, `esp`, `sp` y `spl`.
  - `isWindowsX64NonvolatileRegister()` clasifica GPR y XMM no volátiles.
  - Constraints y clobbers aplican la regla antes de llegar a HIR.
- `include/nativesharp/AST/AstNodeKind.h`
  - Define `flag()` del clobber como obligación de preservación ABI.
- `tests/unit/IntegratedAssemblyGrammarTests.cpp`
  - Certifica marcadores para `rbx`/`xmm15` y rechazos del stack pointer.
- `docs/1.0.1`
  - Actualiza especificación, contratos, mapas, estado, changelog y roadmap.

## Decisiones / Decisions

El compilador no permite ajustes opacos de `rsp`: no puede verificar balance, alineación y unwind
desde texto libre. Los registros no volátiles sí pueden declararse clobbered porque LLVM puede
materializar su preservación cuando reciba el contrato. / Opaque `rsp` adjustment is forbidden
because balance, alignment, and unwind cannot be proven from free text. Nonvolatile registers may
be clobbered because LLVM can preserve them when given the contract.

## Evidencia enfocada / Focused evidence

```text
nsc.integrated-assembly.grammar: PASS: 42 checks
```

No se ejecutó baseline. / No baseline was run.

## Mensaje de commit / Commit message

**Título / Title**

`Protege stack y registros no volátiles en asm / Protect stack and nonvolatile registers in asm`

**Descripción / Description**

`Prohíbe constraints del stack pointer y marca clobbers no volátiles para preservación ABI. / Forbid stack-pointer constraints and mark nonvolatile clobbers for ABI preservation.`
