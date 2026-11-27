# T4a - A note when a template's unit cannot name an argument's record

## Defect

When the translation unit that declares a C++ function template could not name a record argument (no
declaration with its Clang USR), `deduceFunctionTemplateCallWithExplicitLocked` returned `std::nullopt`, the same
answer as "this unit declares no such template". `ProjectCompilation::deduceCppTemplateCalls` then reported
"Clang cannot deduce the template arguments of the C++ function template '...' from this call" with no note, so
the reason was not communicated.

## Cause

`ClangSession.cpp`, `deduceFunctionTemplateCallWithExplicitLocked`: `if (!tag) return std::nullopt;` for a
record argument, after the templates of that name had been found.

## Change

After T5 tried to form the record from its description, a record that still cannot be named answers the request
with the note "argument N has a C++ record type that the translation unit declaring '<name>' neither declares nor
can form", which `deduceCppTemplateCalls` attaches to its diagnostic. The note is added only for namespace-scope
function templates: `@member:` and `@operator:` lookups keep `std::nullopt`, because their callers rely on an
unanswered request to keep their own member or operator diagnostic.

## Preserved behavior

- A unit without a template of that name still answers `std::nullopt`.
- A request another session deduces is unaffected: notes are only reported when no session deduced the call.
- Explicit template arguments keep their previous handling.

## Validation

- NSC built in Release with `/clang:-O3` (`ninja -C build-clang nsc`, exit code 0).
- No tests, baselines or probes were run, as requested; the note was not observed in a compilation.
