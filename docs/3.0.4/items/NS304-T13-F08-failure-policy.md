# NS304-T13-F08 — Failure policy

- **Estado**: PARCIAL — `failure: null|panic|throws` implementados y verificados; `failure: result` bloqueado por defecto de compilador (variant genérico cross-module).
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

## Implementado (2026-10-15)

- `a9b9d266` WIP parser `failure:` + plumbing Sema (`ProjectAllocatorFailureProfile`).
- `75e3b969` `failure: null` — `Allocate` null ⇒ resultado null, ctor no ejecutado.
- `b4a2ce92` `failure: panic` — `Allocate` null ⇒ panic del proyecto (exit 142).
- `b3cf24b1` tipos Core `AllocationFailure` (enum `OutOfMemory`) + `AllocationFailureException : Exception` + `Result<T,E>` (variant `Ok(T)/Error(E)`).
- `5034c0e7` `failure: throws` — `Allocate` null ⇒ `AllocationFailureException` (factory `Create()`); `Throw` admitido como expresión (rama "never" del `Conditional`).
- `e2047eab` casos focales TL-LANG-495 (null), 496 (throws), 497 (panic) — PASS O0.

## Verificado O0

- TL-LANG-495 (null): PASS — null + ctor no ejecutado con allocator fallido; construye normal con válido.
- TL-LANG-496 (throws): PASS — `AllocationFailureException` capturada.
- TL-LANG-497 (panic): PASS — panic exit 142.
- Guard TL-LANG-492 (T13-F07 drop normal): PASS.

## Bloqueo de `failure: result`

El contrato requiere `Result<owned T, AllocationFailure>` como **variant genérica pública en Core**.
Verificado que el compilador **no resuelve un variant genérico desde otro módulo**:

- `Optional<int>` (struct genérico de Core, 1 param) → resuelve cross-module.
- `KeyValuePair<TKey,TValue>` (struct genérico de Core, 2 params) → resuelve cross-module.
- `Result<T,E>` (variant genérico de Core) → error de parse `expected expression` en el consumidor
  (`Result<int,int>` no se reconoce como tipo). El variant genérico **local** sí funciona (tests
  unitarios); el defecto es el cruce de módulo.

Causa raíz apuntada: el nombre del variant genérico no entra en el conjunto de nombres de tipo que el
parser usa para desambiguar `T<...> var` (declaración) frente a una llamada genérica (expresión)
(`Parser::knownTypeNames_` alimentado desde NSMETA). No se maquilló ni se sustituyó por un ABI inventado.

## Residual

- `failure: result` — requiere `Result<T,E>` cross-module; bloqueado por el defecto de variant genérico.
- Regla de vida del allocator del contrato §6 del handover: documentada en T13-F07, aplica igual aquí.

