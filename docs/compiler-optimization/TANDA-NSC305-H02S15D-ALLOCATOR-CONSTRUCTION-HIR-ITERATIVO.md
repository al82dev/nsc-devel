# NSC 3.0.5 — H02S15D: construcción allocator-selected HIR iterativa

## SCOPE LOCK

- Objetivo: retirar la reentrada de pila nativa al bajar receiver y argumentos de una
  `PlacementConstructionExpression` resuelta mediante `IAllocator`.
- Conservado: orden de evaluación, parejas de ordinales sintéticos, layout/trailing, conversiones,
  diagnósticos, forma HIR, ownership, provenance, perfiles de fallo y captura tardía de cleanup.
- Protegido: construcción ordinaria/placement de H02S15E, aggregate/tuple/with de H02S15F,
  parser, Sema, MIR, LLVM, ABI, API pública y NSMETA.
- Prueba única: target `nsc_project_oop_lowering_tests`, selector
  `hir-allocator-construction-depth`.
- Prohibido: CTest, suites, minibaseline, baseline, deploy, instalación y tracking global.

## Implementación

La máquina común reconoce exclusivamente placement válido, no-address, con
`allocatorSelection` y `allocatorAllocateMemberOrdinal`. Su frame reserva `allocator.bytes` y
`allocator.context` en preorder, calcula el tamaño una vez, baja una sola vez el receiver con el
contexto address/value correspondiente y agenda un argumento por reanudación en orden fuente.

El finalizador no baja expresiones, no re-resuelve bindings, no reserva ordinales y no reaplica
conversiones. Materializa contexto, llamada `Allocate`, storage, `PlacementConstruction` y el
wrapper None/Null/Panic/Throws/Result usando el estado ya completado. La rama histórica allocator
conserva sólo el rechazo address y una guarda interna de escape; no contiene reentradas a
`lowerExpression`. La rama ordinaria posterior permanece separada.

## Prueba focal

El selector cubre el contrafactual de 32,768 aristas construction→argument y satélites para cero y
tres argumentos, orden y evaluación única, conversiones, ordinal ausente, allocator class/struct,
rechazo address sin consumo, layouts class/struct, trailing, constructor explícito/implícito,
ownership/provenance, perfiles None/Null/Panic/Throws/Result, errores exactos, cleanup inverso,
retained depth, exception target, filtros de binding y fallo interior con rango profundo.

La revisión focal adicional hace observables los límites que antes sólo estaban implícitos: el
segundo argumento allocator se baja y recibe su conversión no identidad antes del diagnóstico de
ordinal ausente, mientras el tercer argumento no se agenda; el fallo interior atraviesa y
desapila 4,096 frames allocator antes de conservar el rango del `CallExpression`; cada rechazo
comprueba `begin` y `end`; y los satélites de fallo fijan `BangEqual`, el local `byte*`, su ordinal,
el `NullConstant`, los tipos de resultado y de cada rama. Los receivers class/struct comprueban
además el `typeKey` efectivo producido por su lowering, independiente del tipo declarado en el
binding de construcción.

- Build: `A:\NSC-devel-paralelo\logs\agente-1\h02s15d-build-final.log`.
- Focal: `A:\NSC-devel-paralelo\logs\agente-1\h02s15d-focus-final.log`.
- Resultado focal: `PASS: 18 checks`.
- No se ejecuta ninguna otra prueba.

## Criterio de cierre

El target exacto debe compilar, el selector único debe pasar y la auditoría estática debe demostrar
que el finalizador no reentra a ningún lowerer y que la rama allocator histórica no conserva
llamadas recursivas de receiver/argumentos.
