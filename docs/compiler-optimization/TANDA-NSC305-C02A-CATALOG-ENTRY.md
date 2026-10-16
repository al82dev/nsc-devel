# Tanda NSC305-C02a — `serializeCatalogEntry` (entry de catálogo)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: C02 (caché de catálogo: serialización del entry completo, record + AST).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Añadir `serializeCatalogEntry` (record + subárbol AST del nodo syntax). |
| Naturaleza | Añadido (sin cambio de conducta; aún no se consume). |
| Comportamiento idéntico | Sin cambio (codec nuevo). |
| Revisión de referencia | `development` en `961882b5`. |
| Archivos | `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `serializeCatalogEntry`. |
| Pruebas | `Host.Frontend` completo (544 casos, sin regresión). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `serializeCatalogEntry` = record metadata + `serializeAstNodeTree(syntax)`. Los índices se
  reconstruyen de los records restaurados; la restauración (ByteReader) es la siguiente capa.

## 3. Evidencia

- **HECHO COMPROBADO**: compila y `Host.Frontend` **544 PASS / 0 FAIL**.
