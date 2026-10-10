# Tanda 03.01 — Implementación de Host.Dominance

Fecha: 2026-10-09

## Alcance

Se materializaron los `256` casos `TF-DOM-001..256` de los `32` contratos
`CF-DOM-001..032`. El cambio pertenece exclusivamente a la suite nueva:

- no modifica código de producto;
- no toca la batería histórica ni sus seis archivos protegidos;
- no usa `C:`, despliegue, baseline ni remotos;
- no compila ni ejecuta pruebas o gates durante esta etapa.

## Organización implementada

Cuatro módulos registran bloques cohesionados de modelo, construcción,
recorrido, dump y verificación. `DominanceTestSupport.h` contiene el soporte
iterativo común y conserva un caso registrable por ID.

Los contratos que describían validaciones inexistentes en arena o storage se
reescribieron contra el comportamiento real; no se convirtieron en defectos
falsos ni ampliaron el producto.

## Estados visibles

Requieren infraestructura especial posterior:

- `TF-DOM-003`: inyección de fallo de asignación;
- `TF-DOM-008`: contabilidad de destrucción y sanitizer;
- `TF-DOM-068`, `TF-DOM-076`, `TF-DOM-132`, `TF-DOM-140` y `TF-DOM-172`:
  aislamiento de proceso para grafos cíclicos o rotos.

Defecto de producto conservado:

- `TF-DOM-142`: la frontera omite el self-edge por el filtro de predecesores.

## Estado de validación

```text
Implementación: 256/256 MATERIALIZADA
Integración declarativa: PENDIENTE
Build Debug/Release: NO EJECUTADO
Pruebas: NO EJECUTADAS
Gate controlador: NO EJECUTADO
Motivo: validación offline diferida hasta completar toda la Fase 2
```

Los `256` casos aumentan la columna de implementación. Ninguno aumenta la
columna de verificación en esta tanda.
