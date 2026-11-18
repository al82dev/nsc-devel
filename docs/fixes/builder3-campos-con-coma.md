# The comma field-list diagnostic keeps the ExpectedToken identity

NSC Builder run `20261115-233408-dc7151d7`: `TP-PAR-205` (CP-PAR-021), Debug and Release.

- Input: `class C { int first = 1, second = 2; }`.
- Expected by the test: at least one `ExpectedToken` diagnostic.
- Cause: LKF-604 (`d0d9c3494`) replaced the cascade after "expected ';'" with one explanatory
  diagnostic, but reported it as `ExpectedDeclaration`. The comma stands where `;` is expected.
- Change: `src/Parser/MemberParser.cpp`, `parseFieldClassMember`, reports the same message as
  `ExpectedToken`. The message, the single report and the skip of the list are unchanged.
- Validation: `TP-PAR-205` and the full Host.Parser component pass in Debug and Release;
  `declaration_surface_boundaries.py`, which checks the message, passes.
