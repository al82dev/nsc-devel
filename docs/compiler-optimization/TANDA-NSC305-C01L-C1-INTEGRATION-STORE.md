# Tanda NSC305-C01l — Integración C1 (persistir records)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: C01 (integración: persistir los records recolectados en la caché C1).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Conectar `TemplateParseCache` al modelo: `setTemplateCacheRoot` + `storeTemplateRecords` (persiste la metada del record). |
| Naturaleza | Añadido (sin cambio de conducta cuando la caché está deshabilitada; raíz vacía). |
| Comportamiento idéntico | Sin caché configurada, no hace nada. |
| Revisión de referencia | `development` en `0d70ecab`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`, `src/Frontend/ProjectGenericModel.cpp`, `tests/compiler-suite/CMakeLists.txt`. |
| Símbolos | `setTemplateCacheRoot`, `storeTemplateRecords`, `templateCache_`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `storeTemplateRecords` serializa cada record (`serializeTemplateRecord`) y lo guarda bajo
  `c1CacheKey` cuando la caché está habilitada. El `restore` (hit WARM) es la siguiente capa.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
