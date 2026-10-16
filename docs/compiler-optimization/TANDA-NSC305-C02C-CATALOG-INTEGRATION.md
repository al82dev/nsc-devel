# Tanda NSC305-C02c — Integración catálogo (store/restore del snapshot completo)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: C02 (caché de catálogo: persistir/restaurar el snapshot completo record+AST).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | `storeTemplateRecords` guarda el entry completo (`serializeCatalogEntry`); `restoreTemplateRecords` valida el round-trip completo (`restoreCatalogEntry`). |
| Naturaleza | Añadido (sin cambio de conducta con caché deshabilitada). |
| Comportamiento idéntico | Sin caché configurada, no hace nada. |
| Revisión de referencia | `development` en `247692c1`. |
| Archivos | `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `storeTemplateRecords`, `restoreTemplateRecords`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- El payload pasa a ser `serializeCatalogEntry` (record + AST); la restauración valida el round-trip
  completo en la arena de la unidad. El splice AST al árbol de la unidad queda como capa posterior.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
