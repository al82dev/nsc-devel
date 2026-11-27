# T6a - Clang deduction of C++ constructor templates

## Defect

`new glm.vec3(glm.inverse(ViewMatrix)[3])` needs GLM's constructor template
`template<typename U, qualifier P> vec(vec<4, U, P> const& v)`. A C++ constructor template was never a candidate:
the record import (`collectRecords`, `ClangSession.cpp`) keeps only non-template constructors and the
specializations of templates whose parameters all have defaults (`defaultedMemberSpecializations`), and
`deduceFunctionTemplateCallWithExplicitLocked` knew free names, `@member:` and `@operator:` only.

## Change (session side; the frontend uses it in T6b and T6c)

- `deduceFunctionTemplateCallWithExplicitLocked` accepts `@constructor:<record USR>`: the constructor templates of
  that record (any access; T7 checks access on the selected constructor; bases are not searched, constructors are
  not inherited). Deduction, partial ordering and instantiation are the existing ones; partial ordering of
  constructors passes no object type. The USRs of the deduced specializations are kept in
  `deducedConstructorUsrs_`, and the replay after a reparse (`templateDeductions_`) deduces them again.
- `CppRecordDeclaration::hasConstructorTemplates`: the record declares a constructor template.
- `CppRecordDeclaration::deducedConstructors`: the specializations deduced in this session, captured by the
  record import's own `captureMethod` (symbol mangled as the complete-object constructor, parameters, access,
  exception specification). They are a separate list on purpose: `methods` is compared by `sameCapturedMembers`
  between the translation units that define the record, and each unit deduces its own specializations.
  `collectRecords` receives the USR set (new trailing parameter, passed through its recursion).
- `emitCppInlineIR`: a constructor specialization of a function template is offered to the code generator as
  `GlobalDecl(constructor, Ctor_Complete)`, the variant its import names, instead of the generic
  `GlobalDecl(FunctionDecl)` that Clang reserves for non-constructors. Non-constructor specializations unchanged.

## Preserved behavior

- With no `@constructor:` request, `deducedConstructors` is empty and the record snapshot equals the previous one
  apart from `hasConstructorTemplates`, which nothing compares.
- Free, member and operator template deductions are unchanged.

## Pending verification

- No tests, baselines or probes were run, as requested. The deduction is not reachable until T6c records the
  requests.

## Validation

- NSC built in Release with `/clang:-O3` (`ninja -C build-clang nsc`, exit code 0). The build is not a functional
  proof.
