# 363 — Selección y fallback de features de CPU / CPU feature selection and fallback

## Resultado / Result

El target x64 puede seleccionar perfiles reproducibles `x86-64-v1` a `x86-64-v4`. Si el perfil
preferido supera al disponible, se elige el mayor nivel disponible y se registra el fallback. / The
x64 target can select reproducible `x86-64-v1` through `x86-64-v4` profiles. If the preferred
profile exceeds the available one, the highest available level is selected and fallback is recorded.

## Contrato / Contract

- El perfil predeterminado sigue siendo `x86-64-v1`; nunca se consulta ni se asume el CPU del host.
- `TargetInfo` publica la selección, el catálogo de features y si hubo fallback.
- `LlvmTargetMachine` transmite a LLVM el CPU y feature string seleccionados.
- Las pruebas cubren selección AVX2/FMA, fallback v4→v2 y la compatibilidad del baseline v1. /
  The default remains v1, selection is explicit and deterministic, and LLVM receives the resolved
  CPU contract.

## Evidencia enfocada / Focused evidence

`nsc.item363.cpu-feature-selection` valida el contrato directamente. / validates the contract directly.
