# 296 — Control de flujo asm / Assembly control flow

## Resultado / Result

Labels únicos y saltos directos locales, incluidos forward jumps, quedan resueltos en el AST.
Transferencias externas, indirectas y call/return se rechazan. / Unique labels and local direct
branches, including forward jumps, are resolved in the AST. External, indirect, and call/return
transfers are rejected.

## Archivos y funciones / Files and functions

- `include/nativesharp/Diagnostics/DiagnosticId.h`: agrega `InvalidInlineAssemblyControlFlow`.
- `include/nativesharp/Parser/Parser.h` y `src/Parser/Parser.cpp`:
  `validateInlineAssemblyControlFlow()` recolecta labels y resuelve branches en dos pasos.
- `tests/unit/IntegratedAssemblyGrammarTests.cpp`: prueba forward jump, destino externo y `ret`.
- `docs/1.0.1`: actualiza contratos, mapas, estado, changelog y roadmap.

## Diseño / Design

La resolución es local al bloque para impedir saltos alrededor de destructores, `finally` o unwind.
La metadata `label:`/`branch:` evita reinterpretar texto en MIR. / Resolution is block-local so
assembly cannot jump around destructors, `finally`, or unwind. Metadata avoids MIR text reparsing.

## Evidencia / Evidence

`nsc.integrated-assembly.grammar: PASS: 45 checks`. No baseline ejecutado. / No baseline run.

## Mensaje de commit / Commit message

**Título / Title**

`Restringe el control de flujo asm a labels locales / Restrict asm control flow to local labels`

**Descripción / Description**

`Resuelve saltos directos internos y rechaza transferencias que evaden CFG, cleanups o unwind. / Resolve internal direct branches and reject transfers that bypass CFG, cleanups, or unwind.`
