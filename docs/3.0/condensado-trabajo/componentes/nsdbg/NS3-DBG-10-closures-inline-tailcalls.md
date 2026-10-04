# NS3-DBG-10 — Closures, inline y optimización / Closures, inline, and optimization

## Resultado / Result

ES: El stack de NSDBG distingue por primera vez frames físicos e inline sin perder el contexto AMD64
que respalda valores y registros. Los scopes inline seleccionan su contexto CodeView real. Las
closures tienen resumen semántico de capturas, y la clasificación de `Captures` ya no manda cualquier
local que contenga `$` al scope equivocado. O0 presenta frames físicos exactos cuando símbolo y línea
están disponibles; O2 publica que la vista fue optimizada y no inventa tail calls eliminadas.

EN: NSDBG now distinguishes physical and inline frames without losing the backing AMD64 context used
for values and registers. Inline scopes select their actual CodeView inline context. Closures have a
semantic capture summary, and `Captures` no longer misclassifies every local containing `$`. O0
presents exact physical frames when symbols and lines are available; O2 declares optimized views and
does not invent eliminated tail calls.

## Cambios productivos / Productive changes

- `ISymbolProvider::inlineFramesForAddress` consume `SymAddrIncludeInlineTrace`,
  `SymQueryInlineTrace`, `SymFromInlineContextW` y `SymGetLineFromInlineContextW`, bajo el mismo lock
  global requerido por DbgHelp.
- `StackFrame` conserva `physicalFrameIndex`, `inlineContext`, `kind` y `fidelity`. `DebugEngine`
  inserta contextos inline antes de su frame físico y reutiliza los registros/unwind verdaderos.
- `ValueService` usa `SymSetScopeFromInlineContext` para una entrada inline. En lambdas NSC
  (`.lambda#`) separa capturas en el scope raíz del subprograma; además acepta sólo los nombres de
  transporte documentados (`capture.*`, `capture#*`, `$capture$*`, `__capture_*`, `closure.env`).
- La presentación de closures mantiene los nombres fuente y limita el resumen a 16 capturas/512
  bytes. Valores optimizados siguen usando `<optimized out>`.
- DAP expone `nsdbgFrameKind`, `nsdbgFidelity`, `nsdbgPhysicalFrame` y `nsdbgTailCalls`; un frame
  inline usa `presentationHint: subtle`.

- `ISymbolProvider::inlineFramesForAddress` consumes the four synchronized DbgHelp inline APIs.
- `StackFrame` retains physical identity, inline context, kind, and fidelity. `DebugEngine` inserts
  logical inline contexts before their backing physical frame and reuses the real unwind/registers.
- `ValueService` selects inline scope with `SymSetScopeFromInlineContext`; NSC `.lambda#` root-scope
  values become captures, while unrelated source names containing `$` remain locals.
- Closure summaries retain source names and stay bounded. Optimized values remain explicitly marked.
- DAP publishes frame kind/fidelity/physical identity/tail-call visibility as additive fields.

## Tail calls: contrato honesto / Honest tail-call contract

Una tail call real elimina el frame del caller. Si CodeView/DbgHelp no entrega una entrada física o
inline, el debugger no puede reconstruir sus variables con seguridad. NSDBG conserva por ello el
stack verificable y publica `not-reconstructible` en frames optimizados. Esto es información útil y
determinista, no un frame falso. / A real tail call removes the caller frame. If CodeView/DbgHelp
does not provide a physical or inline entry, its variables cannot be reconstructed safely. NSDBG
therefore retains the verifiable stack and publishes `not-reconstructible` for optimized frames.

## Archivos / Files

- `include/nsdbg/DebugEngine.h`, `include/nsdbg/Symbols.h`, `include/nsdbg/Variables.h`
- `src/core/DbgHelpSymbolProvider.cpp`, `src/core/DebugEngine.cpp`
- `src/core/FramePresentation.cpp`, `src/core/NativeSharp3Visualizers.cpp`
- `src/core/TypeService.cpp`, `src/core/ValueService.cpp`, `src/dap/DapSession.cpp`
- `tests/NativeSharp3FrameTests.cpp`, `CMakeLists.txt`, `docs/DAP_CONTRACT.md`

## Pruebas puntuales / Targeted tests

- `A:\nsc-debugger\build\nsdbg_ns3_frame_tests.exe` — PASS.
- `A:\nsc-debugger\build\nsdbg_value_service_tests.exe` — PASS.

No se ejecutó baseline, mini-baseline, CTest general, instalación ni deploy. / No baseline,
mini-baseline, broad CTest, installation, or deployment was run.
