# NS304-T16-F06 — Self-host parity

- **Estado**: AUDITADO (selfhost nsc existe; parity estructural por feature sin cerrar)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar self-host parity*.
Plan T16-F06 (DeepSeekDocs): *portar parser, AST, binding, diagnostics y lowering necesarios a la fuente
self-hosted; ejecutar corpus idéntico host/self-host; comparar éxito, código diagnóstico, span,
HIR/MIR/ABI/metadata estructurales; diferencia reabre el ítem causal*.

Estado real:

- `selfhost/nsc/**` + bootstrap/equivalence existen; el corpus idéntico host vs self-host por feature
  T03–T15 (éxito/span/HIR/MIR/ABI/metadata estructurales) no está cerrado.

## Gap

Falta el port self-hosted de cada feature + el corpus idéntico host/self-host + la comparación
estructural (éxito/diagnóstico/span/HIR/MIR/ABI/metadata), sin excepción permanente.

## Nota

F06 depende de F03 (producer/consumer) + el selfhost existente. No es una surface nueva; es la parity
estructural host vs self-host.
