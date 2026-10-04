# NS3-T11-F05 — Capacidades comptime reproducibles / Reproducible comptime capabilities

## ES

`#capability("file", "ruta")` y `#capability("env", "NOMBRE")` autorizan entradas concretas para
`file_i64` y `env_i64` dentro de `#run`. Sin declaración, la lectura falla. Las rutas son relativas
al directorio fuente, se normalizan y no pueden escapar mediante `..` ni ser absolutas. Cada valor
externo genera una dependencia con identidad lógica y SHA-256 del contenido; esa dependencia entra
en la clave de caché y en el digest reproducible, nunca la ruta absoluta del host.

La compilación productiva usa perfil hermético por defecto. Reloj, aleatoriedad y FFI se rechazan
aunque se declaren; un proveedor no hermético queda fuera del perfil de conformidad. Locale, zona
horaria y directorio físico no afectan una evaluación que sólo consume entradas declaradas.

## EN

`#capability("file", "path")` and `#capability("env", "NAME")` authorize concrete inputs for
`file_i64` and `env_i64` inside `#run`. Reads fail without a declaration. Paths are source-directory
relative, normalized, and cannot escape through `..` or absolute spelling. Every external value
produces a logical identity plus a SHA-256 content dependency; it participates in both cache keys
and reproducibility digests, while absolute host paths never do.

Production compilation is hermetic by default. Clock, randomness, and FFI remain rejected even if
declared; a non-hermetic provider is outside the conformance profile. Locale, timezone, and physical
checkout path cannot affect evaluation that consumes only declared inputs.

## Evidencia / Evidence

`nsc_ns3_t11_f05_comptime_capabilities_tests` covers allowed/denied reads, cache invalidation,
root-independent builds, environment inputs, path confinement, hermetic clock rejection, and
ProjectCompilation persistence.
