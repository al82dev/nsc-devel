# Corrección del plan contra capacidades reales

Fecha: 2026-10-09

## Regla aplicada

La suite de Fase 2 caracteriza y verifica el compilador que existe. No convierte
una expectativa documental en una obligación de implementar producto nuevo. A
la vez, una capacidad existente con resultado incorrecto conserva su oráculo
correcto y puede revelar un defecto real.

## Auditoría

| Caso | Evidencia pública | Decisión |
|---|---|---|
| `TL-LANG-034` | `ClosureLayout` contiene sólo `size` y `alignment`; `computeClosureLayout` devuelve esos campos. | Retirar offsets del oráculo y probar exactamente tamaño/alineación. |
| `TL-LANG-201` | `IntegerArithmeticOperation` contiene Add, Subtract, Multiply, Divide, ShiftLeft y ShiftRight. | Retirar remainder y probar la semántica completa de Divide. |

No se encontró una declaración pública, implementación vigente ni historia del
producto que hiciera parte del contrato actual a los offsets o remainder.

## Diferencia frente a los siete defectos

Los siete defectos documentados ejercitan operaciones que sí existen y violan
un contrato observable: overflow de layouts, enums fuera de dominio, dominancia
de conjuntos, escape numérico y alineación final. Sus pruebas no se reescriben
para aceptar el defecto. Permanecerán rojas hasta corregir el producto.

## Resultado

- Casos inventados eliminados: 0.
- IDs conservados y reescritos contra capacidad real: 2.
- Features añadidas al producto: 0.
- Denominador de la ola 1: 747, sin cambio.
