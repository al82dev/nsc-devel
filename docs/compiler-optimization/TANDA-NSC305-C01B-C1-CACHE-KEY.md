# Tanda NSC305-C01b — `c1CacheKey` (clave C1 completa)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: C01 (caché de templates parseados: clave completa con versión de esquema).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Añadir `c1CacheKey` (SHA-256 de sourceFingerprint + versión de esquema C1) y la constante de esquema. |
| Naturaleza | Añadido (sin cambio de conducta). |
| Comportamiento idéntico | Recogida de templates (una clave extra). |
| Revisión de referencia | `development` en `7bd3c7c9`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`, `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `GenericTemplateRecord::c1CacheKey`, `templateCacheKey`, `templateCacheSchema`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `templateCacheKey` = SHA-256(sourceFingerprint + `nsc-c1-template-v1`); se guarda en `c1CacheKey`.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
