# T6b - Deduced C++ constructor specializations as members, without moving ordinals

## Defect

The C++ constructors of a record are projected once, in `ProjectCompilation::buildMemberModel`
(`provideExperimentalCppLifetimeMethods`), before the method bodies whose constructions drive template deduction.
The deduction rounds of `compileProjectMethodBodies` rebuild only the namespace functions, so a constructor
template specialization deduced for a construction (T6a) had no way to become a constructor.

## How the members are incorporated (determined before changing it)

- Ordinals: other models keep member ordinals and per-ordinal tables built before the bodies
  (`ProjectVirtualDispatchModel::virtualInfoByMember_`, the layout and virtual tables of
  `ProjectCompilationIndexes`); their lookups are bounds-checked. The method body model, the construction model
  (its demand resolver), HIR and MIR are built after the rounds or again in each round, and
  `namespaceCallableBase_` is taken from `members().size()` at each body build. Appending at the end therefore
  keeps every existing ordinal valid, and a new ordinal only has to be visible to the models built afterwards.
- Indexes by type: `membersByType_` and `constructorsByType_` only get the new ordinal at their end;
  `membersDeclaredByType_`, `membersByName_` and the user-conversion index are rebuilt from `members_`.
- References: `ProjectCompilationIndexes::membersByOrdinal_` holds pointers into `members_`, which an append may
  move, so it is re-pointed (`appendMembers`) without clearing the layout, virtual or HIR tables that
  `rebuildMembers` clears.
- Cross-unit records: the deduced specializations are not added to `CppRecordDeclaration::methods`, which
  `sameCapturedMembers` compares between the units that define the record; they travel in `deducedConstructors`
  (T6a) and are merged from every unit of the record's group.

## Change

- `CppSemanticFunction.cpp`: `collectCppSemanticLifetimeFamilies` projects each deduced specialization of every
  observation of the group (deduplicated by USR) with `projectCppSemanticLifetimeMethod`, as the record's other
  constructors, into `CppSemanticLifetimeFamily::deducedMethods`. One whose NativeSharp signature
  (`cppLifetimeSignature`, the signature the family already used, now one function) a projected constructor has is
  left out: C++ prefers the non-template on an identical signature. Records with virtual bases keep them out.
  `provideExperimentalCppLifetimeMethods` returns them with `templateSpecialization`, after the family's methods, so
  only accepted families contribute them.
- `CppImportedLifetimeMethod::templateSpecialization` and `ClassMemberSymbol::cppTemplateSpecialization` carry
  the flag.
- `ProjectMemberModel::addCppLifetimeMember`: the body of the build loop that turned an imported lifetime method
  into a member, moved unchanged into one function (plus the flag).
- `ProjectMemberModel::appendCppLifetimeMethods`: after `build`, appends constructors not yet members of their
  record (same owner and symbol), skips silently one whose parameter types cannot be members, and rebuilds the
  indexes as described. `ProjectCompilationIndexes::appendMembers` re-points the member table and indexes the new
  names and signatures.
- `CppImportedTypeDeclaration::cppHasConstructorTemplates`, from `CppRecordDeclaration::hasConstructorTemplates`
  (`CppSemanticRecord.cpp`), so constructions know when to ask for a deduction (T6c).

## Preserved behavior

- `build` creates the same members in the same order (the loop calls the moved function).
- Without deduced specializations no family has `deducedMethods` and nothing is appended.
- A family that was rejected (deferred, signature collision, blocking siblings) contributes nothing, as before.

## Pending verification

- Not built: the C++ toolchain at `C:/clang-llvm/bin` that `build-clang` uses was no longer present when this was
  built (see the final report). No tests, baselines or probes were run, as requested.
