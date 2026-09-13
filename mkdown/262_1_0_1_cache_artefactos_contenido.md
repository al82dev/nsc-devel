# Entrega 262 — Cache de artefactos por contenido / Content-addressed artifact cache

## Estado / Status

- Tanda / Batch: 1 — acelerar el ciclo de desarrollo.
- Item: 262.
- Resultado / Result: EXE, static-library y DLL pueden reutilizar unidades COFF verificadas mediante
  un cache explícito cuya identidad deriva del contenido y de toda la política de emisión.
- Baseline: reservado para el cierre de la Tanda 1.

## Diseño e implementación / Design and implementation

`CompilationArtifactCache` owns no global directory and is disabled by default. The Driver creates
one invocation-local policy and enables it only after accepting
`--artifact-cache=<absolute-directory>`. A scoped pointer lets the multi-object generator observe
that policy without expanding every product-builder signature or leaking it into nested compiler
invocations.

La clave `NSCOBJKEY 1` incluye identidad exacta del build de `nsc`, versión LLVM cargada, triple,
data layout, nivel O0–O3, modo de debug y el texto LLVM completo después de optimizar. El SHA-256 de
ese material define el subdirectorio y nombre. Un cambio de fuente que cambie IR, una política de
debug/optimización distinta, otro backend o cualquier compilador reconstruido produce otra clave.

Each cache object has an `NSCOBJ 1` sidecar binding the input key to object byte size and SHA-256.
Restore verifies that sidecar and object bytes, copies the candidate into the invocation's temporary
directory, then runs the existing COFF verifier. A malformed, partial, corrupt or structurally
invalid entry is never linked: it becomes a miss and normal emission replaces it with verified
bytes.

Parallel emission keeps byte-sized hit and miss slots per canonical module ordinal. No worker
updates a packed shared bit or a common counter. The coordinator totals the slots after every worker
finishes, and EXE/LIB/DLL reports expose `artifact-cache-hits` and `artifact-cache-misses`.

## Archivos / Files

- `include/nativesharp/LLVM/CompilationArtifactCache.h`: cache policy, key, restore/store and scoped
  invocation access.
- `src/LLVM/CompilationArtifactCache.cpp`: SHA-256 identities, versioned sidecars, integrity checks
  and cache materialization.
- `include/nativesharp/LLVM/ProjectOopObjectGenerator.h`: observable hit/miss counts.
- `src/LLVM/ProjectOopObjectGenerator.cpp`: per-unit restore, COFF verification, miss emission and
  verified store.
- `src/Driver/Driver.cpp`: option parsing, invocation scope, help and product statistics.
- `tests/unit/CompilationArtifactCacheTests.cpp`: key sensitivity, store/restore, corruption and
  nested policy scopes.
- `tests/unit/CompilationArtifactCacheDriverTests.cpp`: CLI help and invalid-policy boundaries.
- `tests/unit/ProjectOopLoweringTests.cpp`: real multi-unit all-miss followed by all-hit generation.
- `CMakeLists.txt`, compiler contract, conformance map and roadmap: build integration and traceability.

## Decisiones / Decisions

1. Opt-in cache: normal compilation has no new persistent filesystem side effect.
2. Exact build identity in every key: objects never cross an unreviewed compiler implementation
   change during the development line.
3. Optimized LLVM as content authority: timestamps and project filenames are irrelevant.
4. Digest plus COFF verification: a matching filename is never trusted by itself.
5. Invocation-owned restored copies: linkers never hold cache files open and cleanup remains local.
6. Per-unit cache: unchanged partitions can be reused independently.
7. Explicit hit/miss reporting: users can measure whether the cache actually accelerated a build.

## Aceptación / Acceptance

- Cache unit test passes 15 checks.
- Driver boundary test passes 4 checks.
- OOP lowering passes 85 checks, including a first all-miss and second all-hit multi-object build.
- Una compilación CLI real de `Stage51String` reporta 0 hits/5 misses; la repetición reporta
  5 hits/0 misses y el ejecutable termina con código 0.
- La reconstrucción limpia de `nsc` pasa; no hay baseline intermedio.

## Commit de referencia / Reference commit

**Título / Title**

`Agrega cache verificado de objetos por contenido / Add verified content-addressed object cache`

**Descripción / Description**

`Add an opt-in build-scoped object cache keyed by compiler, LLVM, target, optimization, debug policy
and optimized IR; authenticate stored bytes with versioned SHA-256 sidecars, reverify restored COFF,
integrate per-unit parallel hit/miss accounting, and expose deterministic CLI controls and reports.`
