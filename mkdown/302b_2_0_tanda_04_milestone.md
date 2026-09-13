# Milestone — NativeSharp 2.0 tanda 4 / Batch 4 milestone

## Alcance / Scope

La tanda 4 completa los ítems 287–302 del roadmap y entrega ensamblador integrado x64 de extremo a
extremo: gramática, preservación de texto y ubicaciones, operandos de entrada y salida, restricciones,
inmediatos, memoria, clobbers, seguridad del stack y registros no volátiles, control de flujo local,
validación por target, representación HIR/MIR, lowering LLVM, barreras de optimización, stepping PDB
y certificación ABI desde hosts C y C++. / Batch 4 completes roadmap items 287–302 and delivers
end-to-end x64 integrated assembly: grammar, text and source-location preservation, input and output
operands, constraints, immediates, memory, clobbers, stack and nonvolatile-register safety, local
control flow, target validation, HIR/MIR representation, LLVM lowering, optimization barriers, PDB
stepping, and ABI certification from C and C++ hosts.

El cierre también alinea las barreras explícitas del contrato del toolchain y del baseline de
migración con MIR 1.2, la versión requerida para transportar los efectos de asm sin pérdida. / The
closure also aligns the explicit toolchain-contract and migration-baseline guards with MIR 1.2, the
version required to transport assembly effects without loss.

## Baseline certificado / Certified baseline

```text
Run: 20260913-134437-fe4c58e5
Compiler build: 1.0.1-dev+build.20260913T194438Z.9678947893c35f0a
Compiler SHA-256: 31f4470a9e3889d0c1e8f67143d739bf0995206ae9d3f63bf5289afcdb5f879b
lld-link CTest: 340/340 passed
NSLink CTest: 340/340 passed
lld-link fixtures: 148/148 passed (O0/O2)
NSLink fixtures: 148/148 passed (O0/O2)
lld-link bug triage: 18/18 passed (O0/O2)
NSLink bug triage: 18/18 passed (O0/O2)
Gate 0: LANDED
Elapsed: 288.561 s
```

La evidencia canónica queda en `_abi/gates/G00/gate.json`; el historial detallado de la corrida es
regenerable y no forma parte del snapshot. / Canonical evidence lives in
`_abi/gates/G00/gate.json`; the detailed run history is regenerable and is not part of the snapshot.

## Decisión de snapshot / Snapshot decision

El branch `nativesharp-2.0-tanda-04` apunta al commit que incorpora esta evidencia. El trabajo
continúa sobre `development`; el branch es un hito inmutable, no una rama de implementación. / The
`nativesharp-2.0-tanda-04` branch points at the commit carrying this evidence. Work continues on
`development`; the branch is an immutable milestone, not an implementation branch.

## Mensaje de commit de referencia / Reference commit message

**Título / Title**

`Certifica el milestone de la tanda 4 / Certify the batch 4 milestone`

**Descripción / Description**

`Registra el Gate 0 verde para la tanda de ensamblador integrado con 340 pruebas, 148 fixtures O0/O2 y 18 casos de triage por cada linker, y prepara el snapshot inmutable. / Record the green Gate 0 for the integrated-assembly batch with 340 tests, 148 O0/O2 fixtures, and 18 triage cases per linker, and prepare the immutable snapshot.`
