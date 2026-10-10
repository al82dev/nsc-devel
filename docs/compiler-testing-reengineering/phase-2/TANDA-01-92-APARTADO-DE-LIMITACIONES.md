# Tanda 01.92 — apartado explícito de limitaciones

## SCOPE LOCK local

| Dimensión | Decisión |
|---|---|
| Ola 1 | conserva `743/747` implementados |
| Limitaciones | `TS-SRC-008`, `TS-SRC-040`, `TS-SRC-041`, `TS-SRC-049` |
| Estado | pendientes de infraestructura especial |
| Ola 2 | autorizada para avanzar por independencia causal |
| Producto | no modificar |
| Suite histórica | conservar intacta; no ejecutar |
| Prohibido | baseline, cobertura, `C:`, despliegue y remotos |

## Decisión

Las limitaciones no se transforman en éxitos, defectos del producto ni casos
implementados. Se conservan en el denominador de la ola 1 y en el documento
canónico de pendientes. El trabajo tangible e independiente continúa con
`Host.Lexer`, primer componente de la ola 2.

Esta decisión separa tres estados que no deben confundirse:

1. `PASS`: comportamiento verificado.
2. `KNOWN_FAILURE`: prueba implementada que revela un defecto vigente.
3. `PENDIENTE DE INFRAESTRUCTURA`: prueba todavía no implementable de forma
   determinista bajo el SCOPE LOCK actual.

La ola 1 continúa `EN CURSO`; no se declara cerrada ni completa. La ola 2 también
queda `EN CURSO` porque sus contratos no dependen de esas cuatro fronteras.
