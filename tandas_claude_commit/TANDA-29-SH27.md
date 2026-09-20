# Tanda 29 — SH27 — commit

**Estado:** compilado; gate dirigido aprobado; listo para commit.

## Archivos

Modificados: `CMakeLists.txt`, `selfhost/nsc/CodeGen/LlvmBackend.ns`,
`selfhost/nsc/Driver/Program.ns`, `docs/2.0/SELF_HOSTING_ROADMAP.md`.

Nuevos: contrato y verificador SH27, gate CMake, expediente, este archivo y las cuatro piezas del
proyecto `selfhost/tests/backend-general/`.

No entran `selfhost/out`, objetos, ejecutables, response files ni logs.

## Commit

`selfhost(tanda-29): generalize the project LLVM backend`

SH27 replaces first-source project emission with one canonical root-program module, removes the
four-argument backend ceiling, and derives stable COFF names from qualified semantic identity and
nominal function type. The directed gate executes a three-unit, six-argument program with LLD and
NSLink in O0–O3; every route returns 42 and exposes identical mangled names.
