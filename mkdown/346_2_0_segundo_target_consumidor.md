# Ítem 346 — Segundo target guiado por consumidor / Consumer-driven second target

## Resultado / Result

La arquitectura `arm64` que ya seleccionan los manifiestos SDK dispone ahora de una identidad de
target concreta: `aarch64-pc-windows-msvc`, ABI Windows ARM64, baseline `armv8-a`, catálogo propio
de features/registros y modelo de datos LLP64. El backend inicializa AArch64 en LLVM y ofrece un
`TargetMachine` Windows ARM64. / The `arm64` architecture already selected by SDK manifests now
has a concrete target identity: `aarch64-pc-windows-msvc`, Windows ARM64 ABI, `armv8-a` baseline,
its own feature/register catalog, and the LLP64 data model. The backend initializes LLVM AArch64
and exposes a Windows ARM64 `TargetMachine`.

## Alcance honesto / Honest scope

El target adicional sirve al consumidor de emisión de objetos portables. No se anuncia todavía
como target ejecutable integral de `nsc`: runtime, linker, ABI, PDB y ejecución continúan bajo el
contrato x64 hasta contar con sus matrices ARM64. Así se evita convertir una etiqueta aceptada en
una promesa multiplataforma falsa. / The additional target serves the portable object-emission
consumer. It is not yet advertised as a complete executable `nsc` target: runtime, linker, ABI,
PDB, and execution remain under the x64 contract until ARM64 matrices exist. This prevents an
accepted label from becoming a false multi-platform promise.

## Pruebas / Tests

La prueba focalizada valida identidad, baseline, registros, LLP64 y emisión LLVM, luego lee los dos
primeros bytes del COFF y exige `0xaa64` (`IMAGE_FILE_MACHINE_ARM64`). / The focused test validates
identity, baseline, registers, LLP64, and LLVM emission, then reads the first two COFF bytes and
requires `0xaa64` (`IMAGE_FILE_MACHINE_ARM64`).
