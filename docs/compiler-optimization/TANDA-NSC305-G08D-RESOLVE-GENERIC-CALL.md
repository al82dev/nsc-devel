# Tanda NSC305-G08d — `resolveGenericCall` (resolución de llamada compartida)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: G08 (cuarto paso: extraer la resolución de llamada genérica).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Extraer `resolveGenericCall` (named + shared + unique) de `processGenericCall`, devolviendo `GenericCallResolution`. |
| Naturaleza | Extracción pura (sin cambio de conducta). |
| Comportamiento idéntico | Resolución de llamadas, ambigüedad y fallback. |
| Revisión de referencia | `development` en `2ffe2636`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`, `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `resolveGenericCall`, `GenericCallResolution`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `GenericCallResolution` (named/shared/unique) y `resolveGenericCall` encapsulan
  `templatesNamedBy` + `collectMethodTemplatesFromNamed` + `resolveMethodTemplate`.
- `processGenericCall` usa el resultado en vez de re-resolver.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
