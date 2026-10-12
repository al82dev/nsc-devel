# NS304-T01-F08 — T01 integration, metadata, and debug

## Contract

T01 closes only when its static constructors, static locals, and condition declarations are
observable through the productive compiler, not only through frontend models. The closure must:

- publish the three capabilities without changing the public ABI projection;
- preserve deliberate down-level NSMETA emission;
- version NativeSharp debug metadata over the existing CodeView/PDB producer;
- emit and execute an EXE and a DLL with full debug information;
- prove concurrent first touch, same-thread constructor reentry, static-local persistence,
  condition-declaration execution, and DLL unload.

## Root cause closed during integration

Partitioned object generation declares every function in the globals coordinator but prunes
unrelated function declarations from ordinary code units. `declareTypeInitializers` correctly
ran after `declareFunctions`, yet it required every code unit to resolve every static-constructor
body. Ordinary units never define type initializers and therefore must only declare the external
coordinator. Body lookup now occurs only in the globals-owning partition; coordinator declarations
remain present in every partition that needs them.

## Versioned surfaces

- NSMETA 1.39 appends `languageFeatureFlags` after the 1.38 payload. The current producer publishes
  static constructors, static locals, and condition declarations. The reader rejects unknown bits.
  A deliberate 1.38 write omits the field and reads it back as zero.
- NSDEBUG 1.1 is a NativeSharp contract layered on CodeView/PDB. Full-debug LLVM modules publish
  major and minor module flags. The integration products require non-empty PDB artifacts.
- The ABI surface projection remains independent of `languageFeatureFlags`; T01 does not alter
  public layout or calling convention.

## Focused evidence

- Compiler build closure: 56/56 steps for the initial integration and 6/6 for the owner-only
  correction, using the existing LLVM/Clang 23 Ninja graph without CMake regeneration.
- Suite build closure: 18/18, 3/3, and 2/2 focused steps.
- `TL-LANG-301`: `PASS` in 805,071 microseconds; the T01 consolidation repeated it in 786,280
  microseconds.
- T01 consolidation: `TL-LANG-294` through `TL-LANG-301`, 8/8 `PASS`.
- Surface guards: `TL-LANG-155` through `TL-LANG-161`, 7/7 `PASS`.
- NSMETA round-trip guard: `TF-INTEROP-273` `PASS` in 26,440 microseconds.
- No full battery was run.
