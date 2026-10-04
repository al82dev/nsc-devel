# NS3-T07-F05 — Loop expressions and value breaks

Status: implemented on `development`
Platform: Windows x64
Validation profile: directed O0/NSLink only; no baseline

## Delivered contract

`loop { ... }` is a NativeSharp 3 value expression. The body repeats after normal fallthrough and
`continue`; it exits normally only through a `break`. Every reachable value break is evaluated
once, converted to the loop's unified result type, stored before lexical cleanup, and then routed
through the existing structured-exit cleanup chain.

The supported labelled forms are `loop name { ... }`, `break name: value;`, and
`continue name;`. An unlabelled exit targets the innermost applicable construct. A labelled exit
can cross nested loops and switches, but the label must name an active loop expression and active
labels must be unique.

## Type and value rules

- Reachable break values are unified by identity, numeric common type, or an existing implicit
  conversion. A diagnostic names the incompatible pair when no common type exists.
- A divergent `builtin:never` break value does not contribute a fictitious value or exit edge.
- A loop with no reachable value-producing break has type `builtin:never`.
- `break;` contributes `builtin:void`; it cannot be mixed with a non-void break value.
- `while`, `do`, `for`, `foreach`, and `switch` remain statements. A value break that targets one
  of them is rejected.
- The expression is a value, not an addressable place. Move-only results require the ordinary
  explicit transfer rules (`break move(owner);`).

## Ownership, lifetimes, and cleanup

The loop uses the same back-edge ownership checks as statement loops: an owner, owned parameter,
or partially moved field cannot arrive at another iteration in a newly moved state. Break values
participate in the normal ownership flow. A borrowed result rooted in a local created inside the
loop is rejected because that local dies when the loop exits.

The backend spills a value break into loop-result storage before executing `defer`, RAII,
`finally`, or other structured cleanup. The source of a move is therefore disabled by the normal
move analysis, the destination survives cleanup, and exactly one later owner destroys it.

## Pipeline and observable contract

- Dedicated `LoopExpression` AST, HIR, and MIR nodes preserve the construct.
- MIR schema 1.27 publishes loop result type, optional label, labelled break/continue targets,
  converted break values, and cleanup continuations.
- LLVM emits a native body/back-edge/end CFG and result storage only for inhabited result types.
  A `never` loop terminates its synthetic end block with `unreachable` and creates no result slot.
- No runtime object, heap allocation, CLR construct, or hidden linker dependency is introduced.

## Files changed

- `include/nativesharp/AST/AstNodeKind.h`, `src/AST/AstNodeKind.cpp`,
  `include/nativesharp/Parser/Parser.h`, and `src/Parser/Parser.cpp`: syntax and AST.
- `include/nativesharp/Frontend/ProjectMethodBodyModel.h` and
  `src/Frontend/ProjectMethodBodyModel.cpp`: result unification, labels, ownership, lifetime,
  back-edge flow, and structured exits.
- `include/nativesharp/Frontend/ProjectOopHirModel.h`,
  `src/Frontend/ProjectOopHirModel.cpp`, and HIR dumper: typed lowering and break conversions.
- `include/nativesharp/Frontend/ProjectOopMirModel.h`,
  `src/Frontend/ProjectOopMirModel.cpp`, and MIR dumper: schema 1.27 contract.
- `src/LLVM/ProjectOopLlvmLowerer.cpp`: result storage, labelled targets, cleanup-safe exits, and
  native CFG.
- `tests/unit/NativeSharp3T07ErrorTests.cpp`: productive and negative directed evidence.
- T07, language, roadmap, master, and root tracking documentation.

## Directed evidence

- Exact feature target `nsc_ns3_t07_error_tests`: built successfully.
- Exact feature executable: `PASS: 41 checks`.
- Productive O0 executable linked by NSLink exits with 42.
- Productive cases cover plain values, labelled outer breaks, labelled continues, nested `never`
  loops, defer cleanup, and a move-only owner destroyed exactly once.
- Negative cases reject value breaks into statement loops, missing labels, incompatible result
  types, duplicate active labels, and borrows rooted in loop-local storage.

No CTest suite, matrix, baseline, O1/O2/O3 build, installation, deploy, remote operation, or write
to `C:` was performed.
