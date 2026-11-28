# T13 - Declaration checks that need C++ value traits wait for them

## Defect

A static field (or the backing field of an automatic property) whose type is a C++ record with trivial copy,
assignment and destruction was rejected:

    static field 'F' cannot hold C++ type 'R', which is not trivially destructible: nothing runs at process exit
    to destroy it; hold it by pointer or in an instance field

wrapped in `cannot build preliminary members for C enum function demands`. The record is a value: the final
compilation gives it a MIR struct layout.

## Cause

`interop::observeCppRecords` (`CppSemanticRecord.cpp`) builds a `ProjectNameLookup` and, when the program has C
enum dependencies, a preliminary `ProjectMemberModel`, to collect C enum function demands. That pass runs inside
`selectExperimentalCppRecordTypesForCompilation` before the value traits of the live C++ records are attached
(`type.cppValueTraits` and `type.cppValueOperations`, set after `observeCppRecordValueForEntry`). Without them
`ProjectNameLookup::cppTrivialValueEligible` answers false for every live C++ record.

`ProjectMemberModel` checks static fields and automatic-property backing fields of C++ record type with that
query (`MemberInitialization.cpp`). In the preliminary pass the check therefore rejected every C++ record, and the
first member diagnostic aborts the observation.

## Change

- `ProjectNameLookup::setCppValueTraitsPending` / `cppValueTraitsPending` (`ProjectNameLookup.h/.cpp`): a lookup
  can declare that live C++ value traits are not observed yet.
- `observeCppRecords` sets it on its observation lookup.
- The two member checks (static field, automatic-property backing field) do not run while the traits are pending.
  The final member model, built with the observed traits, applies them unchanged.

## Audit of other uses in the same pass

The preliminary member and namespace-function models also call `cppTrivialValueEligible` to decide which C++
methods and functions are projected (`ProjectMemberModel.cpp`) and to classify drop glue
(`OwnershipQueries.cpp`). Those uses report no diagnostic; with pending traits they only leave some C++ members out
of the preliminary model, which collects C enum demands and is discarded. Other uses (`ProjectCompilation.cpp`,
`ProjectObjectLayoutModel.cpp`, body analysis, HIR and MIR) run after the traits are attached. Not changed.

## Preserved behavior

- A static field or automatic property of a C++ record that is not a transport value is still rejected, by the
  final member model.
- Lookups other than the observation lookup are unaffected (the flag defaults to false).

## Validation

NSC built in Release with `/clang:-O3`. No tests or programs were compiled with it.

## Pending

- Compile a program with a static field of a trivial C++ record and C enum dependencies to confirm it is accepted,
  and one with a non-trivial record to confirm the final rejection.
- Not verified: whether C++ members omitted from the preliminary model can hide a C enum demand.
