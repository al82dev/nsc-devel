# Tanda 03.03 — Implementación de Host.Passes

Fecha: 2026-10-09

## Alcance

Se materializaron los `448` casos `TF-PAS-001..448` de los `56` contratos
`CF-PAS-001..056`. El cambio pertenece exclusivamente a la suite nueva:

- no modifica código de producto;
- no toca la batería histórica ni sus seis archivos protegidos;
- no usa `C:`, despliegue, baseline ni remotos;
- no compila ni ejecuta pruebas o gates durante esta etapa.

## Organización implementada

Siete módulos registran protocolo, modelo y análisis, constantes y efectos,
transformaciones, pass manager, dumps y verificación. `PassTestSupport.h`
concentra el soporte común sin recursión. Cada contrato conserva ocho variantes
y cada ID sigue siendo seleccionable de manera individual.

El inventario canónico queda materializado como `392` casos `PR` y `56` casos
`NIGHTLY`.

## Estados visibles

Infraestructura o corpus especial pendiente:

- `TF-PAS-059`, `TF-PAS-060`, `TF-PAS-063` y `TF-PAS-064`: instrumentación
  de lifetime, fallo de asignación, ownership corrupto, identidad de bytes del
  allocator o sanitizer;
- `TF-PAS-398` y `TF-PAS-400`: expansión exacta del corpus exhaustivo de
  constantes, texto no ASCII y escala.

Estos casos permanecen visibles; no se convirtieron en `PASS`, `SKIP` ni
oráculos vacíos.

## Estado de validación

```text
Implementación: 448/448 MATERIALIZADA
Integración declarativa: PENDIENTE
Build Debug/Release: NO EJECUTADO
Pruebas: NO EJECUTADAS
Gate controlador: NO EJECUTADO
Motivo: validación offline diferida hasta completar toda la Fase 2
```

Los `448` casos aumentan la columna de implementación. Ninguno aumenta la
columna de verificación en esta tanda.
