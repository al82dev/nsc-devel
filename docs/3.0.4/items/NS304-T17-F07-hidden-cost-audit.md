# NS304-T17-F07 — Auditar costos ocultos en MIR/assembly

- **Estado**: COMPLETO — auditoría de costos en MIR (informe por función + causas).
- **Rama**: development

## Verificado O0

- `TL-LANG-518` (CL-LANG-020, Host.Language, `nsc_component_tests`): PASS — `stackalloc int[2]` +
  `--explain-project-costs` muestra `cause: stack <- stack-allocation` y **no** `cause: heap <- allocate-and-construct`.

## Hallazgo del audit

- El informe `--explain-project-costs` desglosa por función `stack-bytes-lower-bound`, `arena-sites`,
  `heap-sites`, `copies`, `moves`, `drops`, `dynamic-dispatch`, `locks`, `atomics`, `unwind`,
  `monomorphizations` y las causas (`stack <- stack-allocation`, `heap <- allocate-and-construct`,
  `arena <- placement-construct`, `drop <- cleanup-region`, etc.).
- Un `Main` con sólo `stackalloc` no introduce heap oculto: su causa es únicamente `stack <- stack-allocation`
  (auditoría de costo observada en el MIR, no inferida).

## Contexto

- El hueco era auditar costos ocultos. El mecanismo ya existía (`--explain-project-costs`, ver
  `TL-LANG-499`); aquí se cerró la auditoría negativa: un programa stack-only no genera heap/arena
  ocultos y el costo queda visible en el MIR.
