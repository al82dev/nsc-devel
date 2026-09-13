# Entrega 259 — Optimización O2 paralela / Parallel O2 optimization

## Estado / Status

- Tanda / Batch: 1 — acelerar el ciclo de desarrollo.
- Item: 259.
- Resultado / Result: las unidades LLVM de O1/O2/O3 se optimizan concurrentemente con estado
  aislado y errores deterministas antes de su emisión COFF paralela.
- Baseline: reservado para el cierre de la Tanda 1.

## Diseño e implementación / Design and implementation

The ordered optimization loop introduced as an explicit boundary in item 258 now has one task per
LLVM module. Each task owns its `LlvmOptimizer` and `LlvmOptimizationObservation`; each module also
already owns a dedicated `LlvmTargetMachine` and LLVM context. Concurrent optimization therefore
shares only immutable policy values.

La configuración de target se completa primero y conserva el orden canónico. Después, O1, O2 y O3
envían todos los módulos —coordinador y unidades de funciones— al pool acotado. Cada tarea escribe
solamente su slot de éxito y diagnóstico. El coordinador consume esos slots por ordinal y reporta
siempre la primera unidad lógica que falla, sin importar cuál worker terminó primero.

O0 remains ordered deliberately. Its optimizer invocation performs mandatory legalization such as
the Win64 EH repair, but does not run the expensive release pipeline. Avoiding another pool creation
and barrier keeps the debug path lean while O0 still benefits from parallel LLVM lowering and COFF
emission delivered by item 258.

`ProjectOopObjectBundle::usedParallelOptimization` records the selected path for focused conformance
tests. It is descriptive state, not a semantic switch and not exposed as a language feature.

## Archivos / Files

- `include/nativesharp/LLVM/ProjectOopObjectGenerator.h`: observable bundle fact for focused tests.
- `src/LLVM/ProjectOopObjectGenerator.cpp`: isolated optimizer tasks, success/error slots and stable
  coordinator reduction.
- `tests/unit/ProjectOopLoweringTests.cpp`: O2 parallel-path assertion and linked optimized DLL.
- Compiler contract, conformance map and 2.0 roadmap: normative behavior and traceability.

## Decisiones / Decisions

1. Un optimizador por tarea: no mutable pass-manager state is shared.
2. Un TargetMachine por módulo: target state remains isolated through optimization and emission.
3. Slots `uint8_t`, no `vector<bool>`: concurrent workers never update packed shared bits.
4. Reducción por ordinal: failure timing cannot reorder diagnostics.
5. O0 ordenado: debug builds avoid an optimization-only worker barrier.
6. O1/O2/O3 paralelos: every release-capable pipeline uses available code-unit concurrency.

## Aceptación / Acceptance

- Un bundle O2 con cuatro workers confirma la ruta de optimización paralela.
- Cada unidad optimizada emite COFF válido.
- La DLL O2 conserva llamadas entre unidades y el símbolo exportado.
- La prueba enfocada OOP lowering pasa 84 checks.
- Una invocación real `nsc -j 4 -O2` sobre `Stage51String.nsproj` genera cuatro unidades, enlaza y
  el ejecutable termina con código 0.
- Reconstrucción limpia de `nsc` pasa; no hay baseline intermedio.

## Commit de referencia / Reference commit

**Título / Title**

`Optimiza unidades O2 en paralelo / Optimize O2 units in parallel`

**Descripción / Description**

`Run release LLVM pipelines through isolated per-unit optimizers and target machines, record success
and diagnostics in canonical slots, retain the low-overhead ordered O0 legalization path, and prove
that parallel O2 units link and execute with cross-unit and export identities intact.`
