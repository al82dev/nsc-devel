# NS3-T00-F04 — Contextos de efectos

## Resultado

El compilador tiene un conjunto de efectos extensible y una cadena de contextos anidados. El
contrato productivo `noheap` ya no viaja como una decisión aislada: se materializa como
`Effect::NoHeap` en miembros, HIR, MIR, dumps y la identidad de caché, y el lowering consulta
`EffectContext`. Los efectos `throws`, `async` y `noheap` están definidos como parte del tipo de
función; `unsafe` y `comptime` son contextuales.

## Cambios

- `ClassMemberSymbol` publica el conjunto canónico de efectos y conserva `isNoHeap` sólo como
  proyección compatible.
- HIR y MIR transportan los bits de efectos, también para miembros reconstruidos desde NSMETA.
- la validación de operaciones heap usa el `EffectContext` del miembro activo.
- los dumps muestran `effects=16` para `noheap`.
- la identidad de caché MIR incluye los bits de efectos, evitando reutilizar código bajo un
  contrato diferente.

## Pruebas dirigidas

- `nsc_item321_noheap_context_tests`: `PASS: 16 checks`.
- `nsc_item322_noheap_propagation_tests`: `PASS: 25 checks` con
  `NSC_LINKER=A:\nsc-devel\build-test-nslink\bin\nslink.exe`.
- cobertura: contexto directo, propagación de llamadas/constructores, overrides/interfaces,
  HIR/MIR, NSMETA productor-consumidor y rechazo `NS3030`.
- baseline: no ejecutado.

## Archivos modificados

- `include/nativesharp/Frontend/ProjectMemberModel.h`
- `include/nativesharp/Frontend/ProjectOopHirModel.h`
- `include/nativesharp/Frontend/ProjectOopMirModel.h`
- `src/Frontend/ProjectMemberModel.cpp`
- `src/Frontend/ProjectMemberModelDumper.cpp`
- `src/Frontend/ProjectOopHirModel.cpp`
- `src/Frontend/ProjectOopHirModelDumper.cpp`
- `src/Frontend/ProjectOopMirModel.cpp`
- `src/Frontend/ProjectOopMirModelDumper.cpp`
- `src/LLVM/ProjectOopMirCacheIdentity.cpp`
- `tests/unit/Item321NoHeapContextTests.cpp`
- `tests/unit/Item322NoHeapPropagationTests.cpp`

## Mejora

Los efectos futuros pueden integrarse en una sola representación y participar de cache, metadata,
lowering y tooling sin añadir otro booleano desconectado por cada feature.
