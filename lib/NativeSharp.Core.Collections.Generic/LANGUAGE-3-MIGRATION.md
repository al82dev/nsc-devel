# NativeSharp.Core.Collections.Generic — Language 3

## Scope

- Project: `NativeSharp.Core.Collections.Generic.nsproj`.
- Edition: Language 3.
- Product sources audited: 25.
- Dependency: `NativeSharpCore.lib` through binary `LibraryReference` and NSMETA.
- Target used for verification: Windows x64, O0.

The audit covered the collection implementations, their enumerators, supporting delegates,
comparers, hashing helpers and `KeyValuePair<TKey, TValue>`. Ownership-bearing storage and all
public transfer paths were reviewed together because the generic bodies are materialized by the
consumer from NSMETA.

## Product changes

### Explicit value semantics for key/value pairs

`KeyValuePair<TKey, TValue>` is now a `copy struct`. Language 3 makes user value types move-only
unless `copy` is declared explicitly. A dictionary enumerator returns each pair by value, so the
pair must retain the small snapshot semantics promised by the API.

The type is not `borrowed`: arrays of borrowed views cannot be stored in the snapshot field of
`DictionaryEnumerator<TKey, TValue>`. The fields themselves remain plain non-owning aliases to
the key and value owned by the source dictionary, as documented by the existing contract.

### Non-throwing HashSet insertion

`HashSet<T>.Add` and its internal set-combination insertion paths now use
`Dictionary<T, bool>.TryAdd`. This matches the public `bool Add(...)` contract and avoids leaking
the checked `ArgumentException` effect of `Dictionary.Add` into a non-throwing operation.

### Language 3 fixture

The T46 integration fixture now uses `Dictionary.TryAdd` for insertions whose uniqueness is part
of the fixture setup. It checks the boolean result explicitly, so a duplicate still fails the
test without requiring an unrelated exception contract on `Main`.

## Files changed

- `KeyValuePair.ns`: declared explicit copy semantics.
- `HashSet.ns`: routed non-throwing insertions through `TryAdd`.
- `tests/integration/ns3_t46_collections/Main.ns`: aligned fixture insertion with checked effects.
- `LANGUAGE-3-MIGRATION.md`: recorded scope, rationale and evidence.

No new product source file or dependency was introduced.

## Verification

- Compiler: `C:\native-sharp\bin\nsc.exe`.
- Compiler identity: NativeSharp 3.0.0, build `2a44eedad51a7a9b`.
- Linker: `A:\nslink-devel\build-release\bin\nslink.exe`.
- Library emission: 25 of 25 sources, O0, one binary dependency, COFF archive generated.
- Focused consumer: `tests/integration/ns3_t46_collections/T46Collections.nsproj`.
- Runtime result: `T46Collections.exe` returned `0`.
- Line-length audit: no product line exceeds 120 characters.

No baseline or O1/O2/O3 build was executed.

## Result

Status: verified for the agreed Language 3 O0 component scope. The library emits as an isolated
binary dependency and its focused consumer validates reference ownership, value collections,
dictionary enumeration, hash-set insertion, linked-list transfers and copied strings at runtime.

## Nota de distribución / Distribution note (2026-10-25)

ES: En `nsc-devel/lib` y en la distribución (`C:\native-sharp\lib`) esta biblioteca se entrega como fuente,
junto a `NativeSharp.Core`, sin un `NativeSharpCore.lib` en su carpeta. La `LibraryReference` relativa a
`NativeSharpCore.lib` que trajo la sincronización del 2026-10-05 no existía ahí, así que ni la biblioteca ni
sus consumidores compilaban ("referenced library does not exist"). El proyecto vuelve a referenciar Core como
proyecto: `../NativeSharp.Core/NativeSharp.Core.nsproj`. La referencia binaria sigue siendo la forma de
`nativesharp-libraries`, donde `NativeSharpCore.lib` sí se construye al lado.

EN: In `nsc-devel/lib` and in the distribution (`C:\native-sharp\lib`) this library ships as source, next to
`NativeSharp.Core`, with no `NativeSharpCore.lib` in its folder. The `LibraryReference` to a relative
`NativeSharpCore.lib` brought by the 2026-10-05 sync did not exist there, so neither the library nor its
consumers compiled ("referenced library does not exist"). The project references Core as a project again:
`../NativeSharp.Core/NativeSharp.Core.nsproj`. The binary reference remains the form used in
`nativesharp-libraries`, where `NativeSharpCore.lib` is built alongside.
