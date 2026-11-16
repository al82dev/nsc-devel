# Builder tests that froze contracts changed on purpose (run 20261115-233408-dc7151d7)

Each section is a separate commit. The product changed its contract in the named commit; the test is
brought to the current contract and still checks exact values.

## TL-LANG-474 (CL-LANG-020): interpolation handler

- Observed: "an interpolation handler Finish must take no parameters and return the handler type".
- Cause: LKF-314 (`026356ee0`) checks the handler protocol. The value of the interpolation is the
  result of `Finish` (`buildInterpolationHandlerExpression`), so a `Finish` returning `int` made a
  `Builder` out of an `int`; the test passed only because the two have the same layout.
- Change: the original handler is kept as the rejected control (exact diagnostic). The accepted
  handler returns `Builder` from `Finish` and records that it ran; the program returns 2.
- NativeSharp.Core: the fixture references `lib/NativeSharp.Core/NativeSharp.Core.nsproj`, which only
  forwards to `A:/nativesharp-libraries/stdlib/NativeSharp.Core`; no internal copy is used.

## TP-PAR-247 (CP-PAR-024): user `++` and `--`

- Observed: no `ExpectedDeclaration` diagnostic for `operator ++` / `operator --`.
- Cause: `d00b3a869` (NSC-NCSS-003) made user-defined `++` and `--` declarable.
- Change: the case requires the canonical names, one parameter and no declaration diagnostic. The
  arity rejection stays in TP-PAR-248.

## TB-BAS-067 (CB-BAS-012): sibling collision in an artifact transaction

- Observed: the commit succeeded although a directory occupied the predicted sibling name.
- Cause: LKF-241 (`6f8bd132f`) puts the publishing process id in sibling and backup names
  (`.nsc-transaction-<pid>-<ordinal>`); the test built `.nsc-transaction-<ordinal>`.
- Change: `siblingPath` includes `GetCurrentProcessId()`; the transaction runs in the test process.
  The case again proves the all-or-nothing preflight and the untouched destinations.
