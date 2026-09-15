# 389 — Sanitizers nativos / Native sanitizers

NativeSharp integra AddressSanitizer en su frontera LLVM mediante el pase nativo `asan`. La
instrumentación se aplica únicamente a módulos válidos, configura primero el target Windows x64,
verifica cada pase y vuelve a verificar el IR resultante. El contrato de enlace identifica de forma
estable el runtime `clang_rt.asan_dynamic-x86_64`, disponible con LLVM 23. / NativeSharp integrates
AddressSanitizer at its LLVM boundary through the native `asan` pass. Instrumentation accepts only
valid modules, configures the Windows x64 target first, verifies every pass, and verifies the
resulting IR again. The link contract gives the LLVM 23 runtime a stable identity.

La prueba dedicada compila operaciones de memoria nativa, ejecuta instrumentación real y exige los
hooks de inicialización y reporte de ASan en el IR. / The dedicated test compiles native-memory
operations, runs real instrumentation, and requires ASan initialization and reporting hooks in IR.
