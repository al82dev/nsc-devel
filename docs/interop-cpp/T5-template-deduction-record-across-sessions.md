# T5 - Template deduction with a record specialization another header names

## Defect

`glm.length2(p->pos - CameraPosition)` was rejected with "Clang cannot deduce the template arguments of the C++
function template 'glm::length2' from this call", with no note. The argument is a `glm.vec3`
(`glm::vec<3, float, glm::packed_highp>`), and `length2` comes from the `CppLibraryReference` of
`glm/gtx/norm.hpp`.

## Cause (T4: determined from the code and the headers)

- `ProjectCompilation::deduceCppTemplateCalls` reports the diagnostic without notes only when every session of
  the project returned `std::nullopt` (a session that answers adds its notes).
- `deduceFunctionTemplateCallWithExplicitLocked` (`ClangSession.cpp`) returns `std::nullopt` without a note in
  two situations: no function template of that name in the translation unit, or a record argument whose Clang USR
  `findTagByUsr` does not find in it.
- Each `CppLibraryReference` is its own translation unit. `glm/glm.hpp` and `glm/gtc/matrix_transform.hpp`
  declare no `length2`. The include graph of `glm/gtx/norm.hpp` (41 files, read from `Z:/sdk/include/glm`) declares
  the class template `glm::vec` but never names `vec<3, float, packed_highp>` outside templates: the `vec3`
  typedef is in `ext/vector_float3.hpp`, which it does not include, and the concrete `vec<3, float, ...>` code of
  `type_vec3.inl` and the `*_simd.inl` files is under `GLM_CONFIG_SIMD`. So that unit has no specialization with
  the argument's USR, and the only session with the template returned `std::nullopt`.

This was determined by reading the code and the headers; no compilation was run to observe it. The existing
mechanisms were enough to determine it, so no diagnostic was added for that purpose (see T4a for the note added
for the remaining failure).

## Change

- `CppInteropSession.h`: `CppRecordSpecialization` (a template's qualified name and its template arguments) and
  `CppTemplateArgument::recordSpecialization`; the virtual `describeRecordSpecialization(usr)`, nullopt by
  default.
- `ClangSession.cpp`:
  - `describeRecordSpecialization` / `describeSpecializationLocked`: for a `ClassTemplateSpecializationDecl` the
    unit declares, its template and arguments (types by builtin spelling or record USR, recursively described;
    integral and boolean values with their type, an enumeration by USR). Packs, template template arguments,
    expressions and qualified types are not described.
  - `tagForTemplateArgument` / `formSpecializationLocked`: when `findTagByUsr` does not find a record argument
    and it carries a description, the unit forms the specialization as a C++ unit naming it would
    (`Sema::CheckTemplateIdType` under a SFINAE trap, then `RequireCompleteType` and
    `instantiateNamedSpecializations`, as `instantiateTemplateLocked` does). The formed record is used only when its
    USR equals the argument's, so it is the same C++ type.
  - `deduceFunctionTemplateCallWithExplicitLocked` uses `tagForTemplateArgument` for call and explicit arguments.
- `ProjectCompilation::describeCppRecordArguments`, called by `deduceCppTemplateCalls` for template (not macro)
  requests: each record argument gets the description the first session of the project gives for its USR. The
  description travels with the arguments, so the replay after a reparse (`templateDeductions_`) forms the record
  again.

## Preserved behavior

- A record the unit already declares is found by USR first, as before; the description is used only when it is
  not.
- Member (`@member:`) and operator (`@operator:`) lookups by record USR, class template instantiation
  (`instantiateTemplateLocked`) and CTAD are unchanged.
- No identity rule changes: a formed record must have the argument's USR.

## Validation

- NSC built in Release with `/clang:-O3` (`ninja -C build-clang nsc`, exit code 0).
- No tests, baselines or probes were run, as requested; the build is not a functional proof. Pending: compiling the
  tutorial that calls `glm.length2`, and checking that the deduced specialization is imported with its parameter
  mapped to the same NativeSharp `glm.vec3` (that import path was not changed and was not observed).
