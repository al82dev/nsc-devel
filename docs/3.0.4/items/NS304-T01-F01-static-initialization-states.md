# NS304-T01-F01 — Estados de inicialización estática

## Estado

`CERRADO` el 2026-10-12. Define el dominio de estado compartido por los ítems
posteriores de T01 sin anticipar su protocolo de concurrencia.

## SCOPE LOCK

- Objetivo: definir exactamente `uninitialized`, `initializing`, `initialized`
  y `failed`.
- Productor: `Language3Surface.h`.
- Contrato/prueba: `CL-LANG-012` / `TL-LANG-294`.
- Revisión base: `dbc5e4289` en `development`.
- Protegido: Parser, Sema, HIR/MIR, runtime, CMake, toolchain, Builder y
  configuración.

## Contrato

| Ordinal | Estado | Significado reservado |
|---:|---|---|
| 0 | `Uninitialized` | la inicialización aún no comenzó |
| 1 | `Initializing` | existe una inicialización en progreso |
| 2 | `Initialized` | la inicialización concluyó correctamente |
| 3 | `Failed` | la inicialización concluyó con fallo |

Los nombres externos son minúsculos y estables; un enum externo devuelve
`unknown`. F01 no define aristas válidas, thread ownership, espera, reentrancia,
ciclos, retry ni panic. Esos contratos pertenecen a F04/F05 y no pueden
inferirse de los ordinales.

## Evidencia puntual

- Catálogo activo: una fila única `TL-LANG-294` en manifest, diseño y
  allocation.
- Reconstrucción enfocada de `nsc_component_tests`: 3 objetos y un enlace, sin
  reconfigurar CMake.
- `TL-LANG-294`: `PASS`; cuatro ordinales/nombres exactos y enum externo.

## Gate F01

```text
states = 4/4
duplicate_names = 0
external_enum = unknown
transition_policy_defined_early = 0
focused_case = TL-LANG-294 PASS
```

Siguiente acción determinista: `NS304-T01-F02`, gramática y semántica de
constructores estáticos por tipo.
