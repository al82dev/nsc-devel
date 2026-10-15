# NS304-T14-F04 — Source mapping y macro backtrace

- **Estado**: COMPLETO — source mapping de generados (invocación + plantilla) + backtrace navegable.
- **Rama**: development

## Implementado (2026-10-15)

- `34ced01b` — cierre del hueco auditado:
  - **Source mapping de generados**: `MacroExpansionRecord` publica `definitionOffset` y
    `definitionLength` (la plantilla `#macro` que produjo la expansión) junto a los offsets de
    invocación ya existentes, de modo que una expansión navega a invocación **y** plantilla.
  - **Cadena de expansión**: `macroExpansionBacktrace(expansions)` renderiza la cadena con
    `#expand <nombre> (invocation offset N, template offset M)` — sin spans ficticios.

## Verificado O0

- `TF-FRONT-521..528` (CF-FRONT-066, Host.Frontend, `nsc_component_tests`): PASS — offsets de
  invocación/plantilla, backtrace navegable, plantilla en offset cero, cadena anidada y
  determinismo.
- Unit `nsc_ns3_t11_f04_hygienic_macros_tests`: PASS (11 checks).

## Contexto

- El hueco era el source mapping de los generados (invocación + plantilla) + la cadena de expansión
  en el diagnóstico + la navegación al origen. Los spans/ diagnóstico primario ya existían; aquí se
  cerró el mapping de generados + el backtrace estructurado. La integración de estos offsets en los
  diagnósticos del parser/sema (para un macro expandido) pertenece al binding de T14-F05.
