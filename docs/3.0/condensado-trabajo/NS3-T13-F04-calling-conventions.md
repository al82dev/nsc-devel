# NS3-T13-F04 — Calling conventions as function types

## Español

NativeSharp 3 conserva la convención de llamada dentro de la identidad canónica de cada tipo
función. Las formas admitidas en Windows x64 son:

- `fn* native(...) -> R`: ABI interna de NativeSharp.
- `fn* C(...) -> R` y `fn* cdecl(...) -> R`: dos escrituras de la misma ABI C canónica.
- `fn* stdcall(...) -> R`: identidad fuente `stdcall`; en x64 usa el transporte uniforme de
  Windows, pero no se vuelve asignable a otro tipo por esa coincidencia física.
- `fn* fastcall(...) -> R`: forma histórica conservada con identidad propia.
- `fn* vectorcall(...) -> R`: ABI vectorcall, incluidos registros vectoriales y HVA ya soportados
  por el backend.
- `fn* win64(...) -> R`: contrato Win64 explícito y con identidad propia.

Parámetros, retorno, carrier (`*`/`&`), convención, variadicidad, efectos, nullability y error
tipado participan en la clave estructural. Por ello, dos firmas iguales con convenciones distintas
no son asignables. El programador debe declarar un adaptador explícito; el compilador no genera
thunks silenciosos.

En Windows x64 sólo `cdecl`/`C` y `win64` aceptan `...`. `vectorcall`, `stdcall`, `fastcall` y la
ABI NativeSharp rechazan funciones variádicas en el frontend. Las referencias `fn&` siguen siendo
exclusivamente NativeSharp: una frontera extranjera siempre es un puntero `fn*` visible.

La prueba dirigida `nsc.ns3.t13.calling-conventions` valida identidad, serialización, mismatches,
restricciones variádicas y una ejecución O0 enlazada por NSLink.

## English

NativeSharp 3 retains the calling convention in the canonical identity of every function type.
The Windows x64 spellings are:

- `fn* native(...) -> R`: NativeSharp's internal ABI.
- `fn* C(...) -> R` and `fn* cdecl(...) -> R`: two spellings of the same canonical C ABI.
- `fn* stdcall(...) -> R`: a distinct source identity; Windows x64 uses the uniform physical
  transport, but that coincidence never makes it assignable to another function type.
- `fn* fastcall(...) -> R`: the retained historical form with its own identity.
- `fn* vectorcall(...) -> R`: vectorcall ABI, including the backend's existing vector/HVA rules.
- `fn* win64(...) -> R`: an explicit Win64 contract with its own identity.

Parameters, result, carrier (`*`/`&`), convention, variadicity, effects, nullability, and typed
error all participate in the structural key. Equal signatures with different conventions are
therefore not assignable. The programmer must declare an explicit adapter; the compiler never
invents a silent thunk.

On Windows x64 only `cdecl`/`C` and `win64` accept `...`. The frontend rejects variadic
`vectorcall`, `stdcall`, `fastcall`, and NativeSharp function pointers. Function references
(`fn&`) remain NativeSharp-only, so a foreign boundary is always a visible `fn*`.

The directed `nsc.ns3.t13.calling-conventions` test covers identity, round-tripping, mismatch
rejection, variadic restrictions, and O0 execution linked by NSLink.
