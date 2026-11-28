# T17 - Enums named only by C++ member signatures

## Problem

Enum import is demand-driven. An enum reached the imported type set when the program named it, when a
C function signature used it, when one of its constants was used, or when a global had its type.
An enum that appeared only in the signature of a C++ method or in the type of a C++ field was not imported.
The member model then could not project that member (`projectCppSemanticEnumType` found no imported enum),
and the member silently disappeared from the record: a call to it reported an unknown member.

Typical shapes:

- a method whose result is an enum the program never names;
- a method with pointer-to-enum output parameters (`E* mapping`) that the program passes as null or as the
  address of a local it never declares with the enum spelled out;
- a field of enum type in a record the program uses.

## Rule

The enums named by the method signatures and field types of the C++ records the program uses are imported in
the same way as the enums of a C function signature. Pointer, array and reference layers are unwrapped,
cv-qualifiers are dropped, and only named, non-anonymous enums are taken. An enum whose import has a deferred
error, or whose projected identity collides with another enum, is not added (the existing collision and error
handling applies unchanged).

## Change

`src/Interop/Cpp/CppSemanticRecord.cpp`, `observeCppRecords`: after the signature enums of C functions are
provided, the referenced and expression-candidate record groups are walked; for each method (result and
parameters) and field, the enum type found under the declarator layers is matched against the enum candidates
by project ordinal and identity key and appended to `referencedEnumTypes`.

## Impact

Only adds enums to the imported set; no existing name binding or conversion changes. Members whose projection
failed only because an enum was missing are now projected.

## Status

Implemented and built (Release O3). Behavior pending verification.
