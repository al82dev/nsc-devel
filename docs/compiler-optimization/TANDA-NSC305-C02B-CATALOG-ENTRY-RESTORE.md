# Tanda NSC305-C02b — `restoreCatalogEntry` (entry de catálogo)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: C02 (caché de catálogo: restauración del entry completo, record + AST).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Añadir `restoreCatalogEntry` (lee record + subárbol AST y enlaza `record.syntax`), extraer `readTemplateRecord` (ByteReader). |
| Naturaleza | Refactor no funcional + añadido (sin cambio de conducta). |
| Comportamiento idéntico | Sin cambio (codec nuevo). |
| Revisión de referencia | `development` en `d0ee631b`. |
| Archivos | `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `readTemplateRecord`, `restoreCatalogEntry`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `readTemplateRecord` (ByteReader) compartido por `deserializeTemplateRecord` y `restoreCatalogEntry`.
- `restoreCatalogEntry` lee record + `deserializeAstNodeTree` y enlaza `record.syntax`.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
