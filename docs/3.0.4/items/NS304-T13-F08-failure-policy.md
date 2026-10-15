# NS304-T13-F08 — Failure policy

- **Estado**: PARCIAL (noheap + rechazo de heap allocation operativos; perfil explícito de failure pendiente)
- **Rama**: development

## SCOPE LOCK de reanudación

- **Revisión base**: `b31407edc0b3e49f967cc99dd0c698ef2bdd8609` sobre `development`; el commit
  `06c3a700` (`temporal`) y la secuencia previa de DeepSeek/Codex se preservan sin reescritura.
- **Objetivo**: implementar el perfil explícito de allocation failure de la surface #16
  `new (allocator, failure: panic|null|result|throws) Tipo(...)`, con el contrato decidido en el
  handover (sección 5): null, result (`Result<owned T, AllocationFailure>`), throws
  (`AllocationFailureException`) y panic, más compatibilidad sin `failure:`.
- **Archivos permitidos**: parser de opciones de construcción; `ProjectConstructionSite`/
  `ProjectConstructionModel`/`ProjectMethodBodyModel`; `ProjectOopHirModel`/`ProjectOopMirModel`;
  `ProjectOopLlvmLowerer`; tipos Core nuevos (`AllocationFailure`, `AllocationFailureException`,
  `Result<T,E>`); casos focales `TL-LANG-495..500` y sus dos inventarios + manifiesto; este
  expediente; tracking maestro; `A:\restantes-deepseek.md`.
- **Archivos protegidos**: toolchain/CMake/NSLink/Builder/baseline/instalación en C:/O1/O2/O3;
  versionamiento NSMETA (prohibido reactivar); T13-F02/F07 salvo ruta causal demostrada; T15-F01.
- **Validación autorizada**: recompilar `nsc` y `nsc_component_tests` sólo en targets necesarios;
  ejecutar individualmente los casos focales nuevos y guards causales (TL-LANG-492/493/494 si la
  ruta cambia), siempre O0 y con temporales en `A:`. No batería, no baseline, no deploy.
- **Riesgo técnico principal**: materialización de especializaciones genéricas implícitas y typing
  contextual de la construction expression en el modo `result` (handover §7). No sustituir por ABI
  interno inventado.
- **Criterio de cierre**: contrato implementado, casos focales PASS, expediente/tracking/lista
  coherentes, commit local aislado.

## Resultado (noheap — 2026-10-14)

- **noheap**: el efecto `noheap` rechaza la construcción heap directa (`NS3030: noheap callable 'Make'
  cannot contain direct heap allocation 'object-construction'`) y el capture de closure que asigna
  entorno — ya operativo.

## Residual

- El **perfil explícito** que integra null/Result/`throws`/panic por construcción + el cleanup exacto
  de argumento/storage/subobjetos en todas las salidas.
