# NS304-T14-F07 — Límites y ciclos

- **Estado**: COMPLETO — ciclo de símbolo específico + presupuesto de profundidad separado.
- **Rama**: development

## Implementado (2026-10-15)

- `7e270a01` — cierre del residual:
  - **Ciclo de símbolo**: un macro cuyo cuerpo expande a sí mismo (`#expand <mismo-nombre>` en su
    expansión) se diagnostica con `macro cycle detected: '<name>' expands to itself` en vez de ser
    absorbido genéricamente por el presupuesto de pasos.
  - **Presupuesto de profundidad separado**: `MacroExpansionEngine(maximumExpansions, maximumDepth)`
    añade un tope de anidamiento independiente; superarlo produce
    `macro expansion depth limit exceeded`. La recursión mutua (sin autociclo directo) sigue
    terminando en el presupuesto de pasos con `comptime backtrace`.

## Verificado O0

- `TF-FRONT-529..536` (CF-FRONT-067, Host.Frontend, `nsc_component_tests`): PASS — ciclo directo,
  recursión mutua con backtrace, presupuesto de profundidad, determinismo.
- Unit `nsc_ns3_t11_f04_hygienic_macros_tests`: PASS (13 checks).

## Contexto

- El presupuesto de pasos + backtrace ya operaban (2026-10-14). Aquí se cerró la detección
  específica de ciclo de símbolo y el presupuesto de profundidad separado. El presupuesto de
  **nodos/memoria** explícito del `ComptimeBudget` (steps/bytes/depth) ya existe para la evaluación
  comptime y no se duplicó en el expander textual.
