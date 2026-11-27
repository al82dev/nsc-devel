# T9 - Element, parameter and global addresses of C++ records are not copies

## Defect

Reading a field through an element of a pointer to a non-trivial C++ record, `mesh->mFaces[i].mIndices[0]`
(`aiFace*` field, `aiFace` element, `unsigned int*` field of the element), was rejected:

```text
experimental C++ local cannot copy or transfer a C++ value (raw-element-address of 'type:...::aiFace' under field-value)
```

The expression only identifies the element in place and reads one of its fields; nothing copies the record or
takes ownership of foreign memory.

## Cause

`ProjectCompilation::compileProjectOopHir` (`src/Frontend/ProjectCompilation.cpp`), the HIR validation of C++ local
lifetimes (`experimentalCppLocalLifetimeAnalysisForTests_`), rejects every HIR node typed as a C++ record that is
not trivially copyable unless its kind is in a list of forms that do not copy (constructions, `LocalAddress`,
`FieldAddress`, constructed results, reference results...). HIR represents a place as an address node typed with
the place's own type and a read of the whole object as the `PointerLoad` (or value node) above it
(`finishVariableRawIndex`/`finishArrayRawIndex` and `loadRawIndexAddress` in `HirCompletionAccessFrames.cpp`,
`lowerIndexExpressionImplCase` in `HirExpressionMachine.cpp`). Only the local and field addresses were in the list, so
`RawElementAddress` (the element of `p[i]`, of a C array field or of a flexible array), the address that
`FieldValue` takes as its receiver, was classified as a copy.

## Change

The list also accepts the other address forms of a place: `RawElementAddress`, `ArrayElementAddress`,
`RangeArrayElementAddress`, `ParameterAddress`, `StaticFieldAddress` and `NamespaceStateAddress`. Each one is the
address of an existing object, as `LocalAddress` and `FieldAddress` are.

## Preserved behavior

- A copy of the whole record is the `PointerLoad`, `ArrayElementValue`, `FieldValue`, `LocalValue`... typed as the
  record above the address, and it is still rejected: `aiFace f = mesh->mFaces[i];` stays an error.
- Assignments, local declarations, by-value parameters and temporaries of such records keep their rules.
- Constness, mutability and identity are not changed: the address nodes are lowered as before; only the
  validation accepts them.
- Trivially copyable records and C records were never checked here.

## Pending verification

- No tests, baselines or probes were run, as requested. Pending: compiling the AssImp object loader that reads
  `mesh->mFaces[i].mIndices[k]`, and checking that a whole-element copy is still rejected.

## Validation

- NSC built in Release with `/clang:-O3` (`ninja -C build-clang nsc`, exit code 0). The build is not a functional
  proof.
