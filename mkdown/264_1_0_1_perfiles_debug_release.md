# Entrega 264 — Perfiles Debug y Release / Debug and Release profiles

## Estado / Status

- Tanda / Batch: 1 — acelerar el ciclo de desarrollo.
- Item: 264.
- Resultado / Result: los productos OOP aceptan perfiles nominales reproducibles sin perder el
  control ortogonal de optimización y símbolos.
- Baseline: se ejecuta una sola vez después de este commit, como cierre de la Tanda 1.

## Diseño e implementación / Design and implementation

`CompilationProfile` centralizes the two named policies. `debug` resolves to O0 with full CodeView;
`release` resolves to O2 with line tables so optimized production artifacts keep useful source and
stack attribution. The resolver receives explicit optimization and debug selections separately and
lets each one override only its corresponding default.

El Driver reconoce `--profile=debug` y `--profile=release` en cualquier posición. Rechaza nombres
desconocidos, perfiles duplicados y su uso con acciones que no generan OBJ, EXE, LIB o DLL OOP. Al
usar un perfil imprime el nombre y las políticas efectivas, evitando que un script tenga que inferir
si un override fue aplicado.

Existing `-O0`–`-O3` and `-g0`/`-gline-tables-only`/`-g` semantics remain independent. The option is
therefore shorthand, not a second optimization pipeline or a hidden build mode.

## Archivos / Files

- `include/nativesharp/Driver/CompilationProfile.h`: profile identity, resolved settings and public
  resolver contract.
- `src/Driver/CompilationProfile.cpp`: canonical names, defaults and explicit-override precedence.
- `src/Driver/Driver.cpp`: option parsing, applicability diagnostics, effective-policy reporting and
  help text.
- `tests/unit/CompilationProfileTests.cpp`: pure policy checks, CLI boundaries and real Debug/Release
  OOP object emission.
- `CMakeLists.txt`: compiler and focused-test integration.
- Debug contract, language book, conformance map and roadmap: normative and user-facing reference.

## Decisiones / Decisions

1. Debug is O0/full: it prioritizes stepping and variable visibility.
2. Release is O2/line-tables-only: it preserves production crash attribution at lower debug cost.
3. Explicit flags win independently and independent of order; profiles never reduce expert control.
4. Case-sensitive canonical names avoid aliases that scripts cannot reproduce reliably.
5. Effective policy is observable in normal compiler output whenever a profile is requested.
6. Single-file and inspection actions remain unchanged; profiles describe modern OOP products only.

## Aceptación / Acceptance

- Focused profile test passes 10 checks.
- Unknown and duplicate profiles fail before source compilation.
- Unsupported actions reject profiles instead of silently ignoring them.
- Debug emits a real OOP object and reports O0/full.
- Release emits a real OOP object and reports O2/line-tables-only.
- Explicit O3/g0 overrides a Debug profile in the policy resolver.
- La reconstrucción limpia de `nsc` pasa; no se ejecutó baseline intermedio.

## Commit de referencia / Reference commit

**Título / Title**

`Agrega perfiles de compilación Debug y Release / Add Debug and Release compilation profiles`

**Descripción / Description**

`Add named Debug and Release policies for project OOP products, preserve independent explicit
optimization and debug overrides, reject ambiguous or unsupported uses, report effective settings,
and verify both policy resolution and real object emission.`
