# 1.0.1 — Corpus de regresiones DLL precompiladas / Precompiled DLL regression corpus

## Resultado / Outcome

**ES.** DLL-P0 queda cerrado con un corpus mínimo propiedad de NSC. El gate Stage
46 produce dos DLLs independientes con sus import libraries y NSMETA, verifica
conjuntamente cada tríada, compila un consumidor sin fuentes de los productores y
ejecuta el resultado con código 42.

**EN.** DLL-P0 is closed with a minimal NSC-owned corpus. The Stage 46 gate builds
two independent DLLs with their import libraries and NSMETA, jointly verifies each
artifact triplet, compiles a consumer without producer sources, and executes the
result with exit code 42.

El corpus consolida los cuatro defectos externos recientes:

1. una implementación de interfaz perteneciente a `Box<int>` sólo es demandada
   por su tabla de interfaz y se ejecuta dentro del productor;
2. el consumidor crea las primeras especializaciones `Catalog<T>` con una clase,
   un struct y una interfaz declarados por otro productor NSMETA;
3. un `extern C` público no exportado permanece como dependencia privada y no se
   reconstruye como API NativeSharp del consumidor;
4. el constructor público de `ValueBox<int>` participa en la exportación física y
   en la verificación conjunta DLL/import library/NSMETA.

The corpus consolidates the four recent external defects: interface-table-only
generic demand, declaration origin supplied by a second metadata producer, a
non-exported native dependency that must stay private, and a public concrete
generic struct constructor covered by physical artifact verification.

## Defecto adicional encontrado / Additional defect found

**ES.** La primera corrida roja mostró que `buildPrecompiledPublicSource` sólo
publicaba de forma inicial clases e interfaces. Un struct público que no aparecía
en la firma de un método, como `DllP0.Types.Song`, sí estaba correctamente guardado
en NSMETA pero desaparecía de la API sintética. Al sustituirlo dentro de
`Catalog<Song>`, name lookup reportaba el tipo como desconocido desde el módulo de
la plantilla.

**EN.** The first meaningful red run showed that
`buildPrecompiledPublicSource` initially published only classes and interfaces. A
standalone public struct such as `DllP0.Types.Song` was correctly stored in NSMETA
but disappeared from the synthetic API. Substituting it into `Catalog<Song>` then
made name lookup report it as unknown from the template module.

La corrección publica todo tipo público descrito por NSMETA. La fase de emisión ya
tenía reconstrucción estricta para class, interface, struct, enum y delegate; no se
relajó layout, ABI, accesibilidad ni verificación de artefactos.

The fix publishes every public type described by NSMETA. The emitter already had
strict reconstruction for class, interface, struct, enum, and delegate; layout,
ABI, accessibility, and artifact verification were not weakened.

## Archivos y funciones / Files and functions

### `src/Driver/Driver.cpp`

- `buildPrecompiledPublicSource` incorpora todos los tipos públicos al conjunto
  materializable, incluidos structs autónomos que sólo se usan como argumentos de
  una plantilla importada.
- `buildPrecompiledPublicSource` now includes every public type in the
  materializable set, including standalone structs used only as arguments to an
  imported template.

### `tests/unit/Stage46NativeLibrariesTests.cpp`

- `verifyLibraryArtifacts` ejecuta el verificador público sobre NSMETA, import
  library y DLL.
- `testPrecompiledDllRegressionCorpus` crea `DllP0Generics`, `DllP0Types` y un
  consumidor source-free; obliga además a importar físicamente ambos runtimes y
  ejecuta la ruta de interfaz genérica.
- `verifyLibraryArtifacts` runs the public verifier over NSMETA, import library,
  and DLL.
- `testPrecompiledDllRegressionCorpus` creates `DllP0Generics`, `DllP0Types`, and
  a source-free consumer; it also forces both runtimes to be physically imported
  and executes the generic-interface path.

### `DLL_HANDOVER_AND_ROADMAP.md`

- registra el cierre de DLL-P0 y la incorporación del handover a la rama por
  petición del usuario;
- records DLL-P0 closure and the user-requested incorporation of the handover into
  the branch.

## Decisiones / Decisions

- **ES:** el fixture no usa Saru, DirectX ni otro repositorio consumidor.
  **EN:** the fixture uses no Saru, DirectX, or other consumer repository.
- **ES:** el struct autónomo se publica porque accesibilidad pública en NSMETA es
  el contrato; no necesita ser alcanzable desde un método para existir en la API.
  **EN:** the standalone struct is published because public accessibility in
  NSMETA is the contract; it need not be reachable from a method to exist in the
  API.
- **ES:** el consumidor llama una función mínima del segundo DLL para que la
  validación posterior al enlace compruebe una importación runtime real.
  **EN:** the consumer calls a minimal function from the second DLL so post-link
  validation proves a real runtime import.
- **ES:** no se habilitó materialización de tipos declaradores anidados desde
  NSMETA; esa ampliación no pertenece a DLL-P0.
  **EN:** metadata-backed materialization of nested declaring types was not added;
  that expansion is outside DLL-P0.
- **ES:** no se instaló ni copió producto alguno a `C:\native-sharp`.
  **EN:** no product was installed or copied to `C:\native-sharp`.

## Evidencia / Evidence

Build independiente / Independent build:

```text
A:\NSC-DEVEL-BRANCHES\nsc-devel-dlls\build-clang-lldlink
CMAKE_HOME_DIRECTORY=A:/NSC-DEVEL-BRANCHES/nsc-devel-dlls
```

Pruebas dirigidas / Focused tests:

```text
nsc_stage46_native_libraries_tests:                 151/151 PASS
nsc_project_oop_lowering_tests:                       71/71 PASS
nsc_stage44_generics_monomorphization_tests:           75/75 PASS
nsc_stage48_2_library_metadata_tests:                  98/98 PASS
nsc_stage52_generic_collections_compiler_fixes_tests: 147/147 PASS
```

La primera versión del corpus falló en `Catalog<DllP0.Types.Song>` antes del
cambio de `Driver.cpp`; después del cambio el mismo consumidor compila, importa
ambos DLLs verificados y devuelve 42.

The first corpus revision failed on `Catalog<DllP0.Types.Song>` before the
`Driver.cpp` change; after the fix, the same consumer compiles, imports both
verified DLLs, and returns 42.

## Límites y continuación / Limits and continuation

- No se ejecutó el baseline global; el handover reserva esa corrida para el cierre
  de un lote sustancial.
- La forma nested y la visibilidad transitiva siguen cubiertas por
  `testFirstExternalGenericSpecializationKeepsArgumentDeclarationOrigin` con
  `ProjectReference`; el importador NSMETA todavía rechaza tipos declaradores
  anidados de forma explícita.
- DLL-P1, la matriz cruzada productor/consumidor con lld-link y NSLink, queda como
  el siguiente lote.

- The global baseline was not run; the handover reserves it for a substantial
  batch closure.
- Nested shape and transitive visibility remain covered by
  `testFirstExternalGenericSpecializationKeepsArgumentDeclarationOrigin` through
  `ProjectReference`; the NSMETA importer still explicitly rejects nested
  declaring types.
- DLL-P1, the producer/consumer cross-linker matrix for lld-link and NSLink,
  remains the next batch.

## Mensaje de commit / Commit message

### Título / Title

```text
Consolida las regresiones de DLLs precompiladas / Consolidate precompiled DLL regressions
```

### Descripción / Description

```text
ES: Publica tipos públicos autónomos al reconstruir API desde NSMETA y agrega un
corpus interno que produce, verifica, consume y ejecuta dos DLLs sin fuentes de
los productores.

EN: Publish standalone public types when rebuilding API from NSMETA and add an
internal corpus that builds, verifies, consumes, and executes two DLLs without
producer sources.
```
