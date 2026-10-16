# Tanda NSC305-C02d — Skip del collect (restore del record en el walk)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: C02 (caché de catálogo: restaurar la metada en warm-build y saltarse su construcción).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | `tryRestoreTemplateRecord` restaura el record desde la caché C1/C2 y salta la construcción de metada (el walk sigue encontrando node + parent). |
| Naturaleza | Añadido (sin cambio de conducta con caché deshabilitada). |
| Comportamiento idéntico | Sin caché configurada, igual que antes. |
| Revisión de referencia | `development` en `db7b6457e`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`, `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `tryRestoreTemplateRecord`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `collectTemplates` consulta la caché por template; en hit deserializa el record, enlaza `syntax` al
  nodo ya caminado y evita la construcción de metada (fingerprint + campos + dependencias).

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
