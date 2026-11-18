# Leaving a catch body by an exception, and LLVM's WinEHPrepare

Branch `development-main-fixes`. This closes item 1 of the review of docs 16 to 18: `catch (E e) { throw e; }`
at O2 (doc 17 reported it as `k6-rethrow` and left it uncorrected).

Evidence lives in `A:\nsc-main-fixes-evidencia\correcciones-revision`:

- `rethrow\` holds the probes (copies; the original `m1-07` examples are untouched);
- `rethrow\r7-min-frame\` holds the IR, assembly, unwind tables and the WinEHPrepare dump;
- `fixall.cmd`, `fixo13.cmd`, `inst.cmd` and `wineh.cmd` are the scripts.

## Defect

A local with cleanup is declared before a `try`. The `catch` body then leaves by an exception: a rethrow
(`throw e;`), a new throw (`throw new Boom();`) or a call that throws. At -O1 and above, the destructor of
that local did not run, or ran on a stale pointer.

| Probe | Shape | O0 | O2 before | O2 after |
| --- | --- | --- | --- | --- |
| `r4-trace-frame` | `A a;` before the try, `B b;` inside, `throw e;` | 102021 | fails | 102021 |
| `r4-trace-heap` | the same with `new` | 102021 | 101002 | 102021 |
| `r5-newthrow-frame` | `throw new Boom();` | 102021 | fails | 102021 |
| `r6-callthrow-frame` | `F.Fail(1);` in the catch | 102021 | fails | 102021 |
| `r3-order-frame` / `r3-order-heap` | count and order, normal and exceptional path | 0 | fails / 4 | 0 |
| `k6-orig-frame` / `k6-orig-heap` | the doc 17 example | 0 | fails / access violation | 0 |
| `r0-none` | rethrow with no local | 0 | 0 | 0 |

The heap values before the fix were measured with the campaign-base compiler (see «Earlier revision»). For
the frame probes, "fails" means A's destructor was skipped. The new regression test reproduces both
variants before the fix (see «Regression»).

Each `r4` to `r6` value is `caught * 100000 + drops * 1000 + order`. Order `21` means B is destroyed first,
then A. The probes also pass at -O1 and -O3 after the fix.

## Cause

The frame object's owner slot, `%local0.addr`, is promoted by SROA at -O1 and above into two phis:

- a phi on the try's `catchswitch` block. Its value is the object, or null on the edge where the try had
  already destroyed it;
- a phi on the catch body's `cleanuppad`. It takes that value from the catch body's blocks.

LLVM's WinEHPrepare (LLVM 23.1, `llvm/lib/CodeGen/WinEHPrepare.cpp`) demotes both phis to stack slots:

1. For the catchswitch phi, `insertPHILoads` cannot reload in its own block. It reloads before each user,
   but skips users that are phis on EH pads ("Leave it alone; we'll insert loads and stores for it
   separately").
2. For the cleanuppad phi, `insertPHIStores` stores each incoming value at the end of the incoming block. The
   incoming value is the catchswitch phi itself, coming from an ordinary block of the catch body.
3. `demotePHIsOnFunclets` then replaces every remaining use of the catchswitch phi with poison. The store from
   step 2 becomes `store ptr poison`, and code generation drops it.

The cleanup funclet then reads a slot that the catch path never wrote. In the dump `r7-min-frame\decl-wineh.log`,
the three stores into `%local0.addr.1.wineh.spillslot` read `store ptr poison`. In the final executable, the
catch path has no store to `-0x10(%rbp)`, and the funclet reads that address.

So the shape is valid IR that nsc emits correctly; the value is lost inside LLVM. Changing LLVM is outside the
authorized limits, so the fix goes in nsc's code generation layer, right before LLVM generates code.

## Earlier revision

The installed `nsc` (`C:\native-sharp\bin\nsc.exe`, build hash `4998f7ed435b91d5`, built 2026-10-27 17:31) is
from the campaign base (`152654e5a`, committed 2026-10-27 17:15). It was run read-only, with every output on A:
(`inst.cmd`):

- **heap probes:** `r4-trace-heap` returns 101002 at O2 (102021 at O0), `r3-order-heap` returns 4, and
  `k6-orig-heap` ends in an access violation. The defect is older than this campaign.
- **frame probes:** they do not compile there ("use of unassigned local variable"), because frame objects
  came with this campaign.

## Fix

`src/LLVM/Internal/WinEhCatchSwitchPhiLegalization.cpp`, function `legalizeWinEhCatchSwitchPhiUses`. It runs on
every module as the last step before code generation:

- `LlvmTargetMachine::emit` / `emitPrepared` (paths without ThinLTO);
- `ThinLtoBackend`, through `lto::Config::PreCodeGenModuleHook` (the -O1 to -O3 executables).

For each phi on a catchswitch block that an EH-pad phi takes from a block other than the catchswitch itself:

1. it creates a stack slot;
2. it stores the phi's incoming values at the end of the catchswitch's predecessors, exactly where
   WinEHPrepare puts its own stores;
3. it gives the affected EH-pad phi entries a load of that slot, at the end of their incoming blocks.

Loads go before stores, so a block that is both an incoming block and a predecessor reads the previous value.
The catchswitch phi keeps its other uses, which WinEHPrepare demotes correctly. A shape where a needed block is
itself a catchswitch has no room for a load or a store and is left to LLVM unchanged; no probe produced it.

The C API calls it uses are declared in `LlvmCApi.h`. The codegen cache identity is the digest of
`src/LLVM/*`, so cached objects from before the fix are not reused.

## Regression

`tests/unit/WinEhCatchExitCleanupTests.cpp` (`nsc.main-fixes.wineh-catch-exit-cleanup`) is a new file; no
existing test changed. It builds and runs each program at -O0 and -O2, for frame objects (`A a;`) and heap
objects (`A a = new A();`):

- `Rethrow`, `NewThrow` and `ThrowingCall`: a normal pass, then an exceptional pass. Each checks the rethrow,
  `drops == 2` and order `21`;
- `LoopRethrow`: the try sits in a loop, with one handled iteration and one rethrow. Expected order `2221`;
  without a rethrow, `22221`;
- `Handled`: a control where the catch ends normally.

The executables run natively on Windows (under Wine elsewhere). Before the fix, 6 of 40 checks failed: the
three exit shapes, frame and heap, at -O2. After the fix, 40 of 40 pass.

## Verification

- **Probes:** `fixall.cmd` (O0 and O2) and `fixo13.cmd` (O1 and O3). Every value matches O0.
- **Passing tests:** `nsc.main-fixes.wineh-catch-exit-cleanup`, `nsc.ns3.t06.escape-summaries`,
  `nsc.class-value-origins`, `nsc.stage50_7.borrow-escape-analysis`, `nsc.stage56.c7-definite-assignment`,
  `nsc.stage45.native-exception-runtime` and `nsc.stage45.linker-pe-eh`.
- **Failures that predate this change:**
  - `nsc.stage45-10.eh-raii-reuse-after-catch` and `nsc.stage45-11.eh-cleanup-execution` fail in semantic
    analysis. Their fixtures call a throwing method without catching it or declaring `throws`. The installed
    compiler (campaign base) rejects the same fixture (`preexisting\s4510`).
  - These two suites only execute programs under Wine. On Windows they skip execution, so they never ran the
    O2 rethrow.
  - `nsc_stage45_typed_catch_rtti_tests`, `nsc_stage45_llvm_win64_eh_basic_tests` and
    `nsc_stage45_llvm_cleanup_funclet_tests` do not link: `nsc_frontend` needs `nsc_precompiled`, which their
    link lists lack. The reference already exists at the campaign base. They were not run.

## Limits

- The LLVM defect itself is not corrected or reported upstream.
- A phi whose required block is a catchswitch is left as it was.
