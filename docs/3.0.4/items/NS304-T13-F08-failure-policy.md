# NS304-T13-F08 — Failure policy

- **Estado**: COMPLETO — `failure: null|panic|throws|result` implementados y verificados O0.
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
- **Criterio de cierre**: contrato implementado, casos focales PASS, expediente/tracking/lista
  coherentes, commit local aislado.

## Implementado (2026-10-15)

- `a9b9d266` WIP parser `failure:` + plumbing Sema (`ProjectAllocatorFailureProfile`).
- `75e3b969` `failure: null` — `Allocate` null ⇒ resultado null, ctor no ejecutado.
- `b4a2ce92` `failure: panic` — `Allocate` null ⇒ panic del proyecto (exit 142).
- `b3cf24b1` tipos Core `AllocationFailure` (enum `OutOfMemory`) + `AllocationFailureException : Exception` + `Result<T,E>` (variant `Ok(owned T)/Error(E)`).
- `5034c0e7` `failure: throws` — `Allocate` null ⇒ `AllocationFailureException` (factory `Create()`); `Throw` admitido como expresión (rama "never" del `Conditional`).
- `e2047eab` casos focales TL-LANG-495 (null), 496 (throws), 497 (panic) — PASS O0.
- `f12cd57b` fix del defecto de variant genérico cross-module: `collectTypeSeedsInScope` no sembraba
  `variant` (sólo struct/class/interface/enum/union/trait/opaque type), por lo que `Result<T,E>`
  no entraba en `knownTypeNames_` del parser consumidor.
- `90ee0ec2` `Result.Ok` con payload `owned T` (`Ok(owned T)`), para `Result<owned T, AllocationFailure>`.
- `12c8fa92` `failure: result` completo — síntesis de `Result<T,AllocationFailure>` en parser/fase
  genérica, `Ok`/`Error` factories resueltos en Sema, envoltura en HIR y **fix de provenance**: el
  entry "delete" (field 5) del descriptor hacía free GC incondicional; ahora lee la thunk de
  provenance del header (como `emitRaiiDestroy`) y cae al free GC sólo si no hay thunk. Caso focal
  TL-LANG-498 (result).

## Verificado O0

- TL-LANG-495 (null): PASS — null + ctor no ejecutado con allocator fallido; construye normal con válido.
- TL-LANG-496 (throws): PASS — `AllocationFailureException` capturada.
- TL-LANG-497 (panic): PASS — panic exit 142.
- TL-LANG-498 (result): PASS — `Ok` con allocator válido, `Error` con allocator null.
- Guard TL-LANG-492 (T13-F07 drop normal): PASS.

## Resolución del bloqueo de `failure: result`

El bloqueo previo tenía dos causas, ambas resueltas:

1. **Variant genérico cross-module** (`f12cd57b`): `collectTypeSeedsInScope` (que siembra los
   nombres de tipo para el parser) reconocía `struct/class/interface/enum/union/trait/opaque type`
   pero no `variant` (palabra contextual). Al no sembrar `Result`, el parser consumidor lo trataba
   como llamada genérica y fallaba con `expected expression`.
2. **Provenance del allocator a través del variant genérico** (`12c8fa92`): el drop del payload
   `owned A` de `Result<A,AllocationFailure>` pasaba por el entry "delete" (field 5) del descriptor,
   que liberaba por free GC incondicional. Para un objeto pooled (`new (allocator, ...)`) ese free
   GC corrompía el heap (exit `0xC0000374`). El entry ahora lee `deallocateThunkOffset` +
   `allocatorContextOffset` del header y llama la thunk del allocator; sin thunk cae al free GC.

## Residual

- Regla de vida del allocator del contrato §6 del handover: documentada en T13-F07, aplica igual aquí.
