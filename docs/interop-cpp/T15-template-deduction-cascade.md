# T15 - No deduction diagnostic after an argument already failed

## Defect

A call to an imported C++ function template (or C/C++ function-like macro) is analyzed in two steps: the first
records a deduction request with the argument types and reports "C++ function template 'F' needs its template
arguments deduced from this call"; Clang then deduces and the second analysis resolves the call. When an argument
expression is itself invalid (an unknown identifier, for example) the request is not recorded, but the message was
still reported, so one source error produced a second, misleading one.

## Change

`CallAnalyzer.cpp`, the C++ deduction site: the error count is taken before the arguments are analyzed; when an
argument fails and added its own error, the call returns an invalid result without the deduction or expansion
message.

## Definite assignment

Reviewed: "use of unassigned local variable" after a rejected assignment. `analyzeAssignmentLeft` marks the target
of a plain `=` as assigned before the right-hand side is analyzed (`markAssignmentTarget`), so a rejected
right-hand side does not leave the local unassigned. The diagnostic is reported when a local is read where some
path does not assign it (for example an `if / else if` chain without `else`): a definite-assignment rule, as in
C#, not a cascade. Not changed.

## Preserved behavior

A call whose arguments are valid still records the request and reports the message for the first analysis. An
invalid argument that reports nothing still gets the message.

## Validation

NSC built in Release with `/clang:-O3`. No tests were run.
