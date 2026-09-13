# 300 — Barreras de optimización de asm / Assembly optimization barriers

## Resultado / Result

El contrato asm ya declara explícitamente sus efectos al optimizador. Un bloque no puede desaparecer
por no tener outputs y `memory` conserva el orden de accesos a ambos lados incluso en O2. / The asm
contract now exposes its effects explicitly to the optimizer. A block cannot disappear merely
because it has no outputs, and `memory` preserves access ordering on both sides even at O2.

## Archivos y funciones / Files and functions

- `include/nativesharp/Frontend/ProjectOopMirModel.h`
  - Eleva MIR a 1.2 y agrega resúmenes `inlineAssemblyHasSideEffects`,
    `inlineAssemblyClobbersMemory` e `inlineAssemblyClobbersFlags`.
- `src/Frontend/ProjectOopMirModel.cpp`
  - Deriva los resúmenes una sola vez desde clobbers HIR validados.
- `src/Frontend/ProjectOopMirModelDumper.cpp`
  - Expone el contrato de efectos para herramientas y diagnósticos.
- `src/LLVM/ProjectOopLlvmLowerer.cpp`
  - Verifica consistencia MIR y transmite side effects, `~{memory}` y `~{cc}` a LLVM.
- `tests/unit/IntegratedAssemblyMirTests.cpp`
  - Optimiza una sonda real en O2 y comprueba bloque observable más stores ordenados.
- `docs/1.0.1`
  - Actualiza especificación, contrato, mapas, estado, changelog y roadmap.

## Implementación y decisiones / Implementation and design decisions

Side effect y memory barrier no son equivalentes. Todo asm es observable para impedir DCE, pero
sólo `clobber("memory")` bloquea transformaciones de memoria globales. Esto evita convertir cada
bloque en una barrera excesiva y mantiene optimizaciones legítimas cuando el autor declara operands
precisos. / Side effect and memory barrier are not equivalent. Every asm block is observable to
prevent DCE, but only `clobber("memory")` blocks broad memory transformations. This avoids turning
every block into an unnecessarily strong barrier while preserving optimization around precise
operands.

## Evidencia enfocada / Focused evidence

```text
nsc.integrated-assembly.mir: PASS: 22 checks
O2 operand-free side-effect retention: passed
O2 memory-clobber store ordering: passed
```

No se ejecutó baseline. / No baseline was run.

## Mensaje de commit / Commit message

**Título / Title**

`Fija barreras de optimización para asm / Define optimization barriers for assembly`

**Descripción / Description**

`Expone side effects y clobbers de memoria/flags en MIR 1.2 y certifica que O2 preserve sus dependencias. / Expose side effects and memory/flag clobbers in MIR 1.2 and certify that O2 preserves their dependencies.`
