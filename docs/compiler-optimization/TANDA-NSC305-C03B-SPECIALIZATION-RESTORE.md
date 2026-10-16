# Tanda NSC305-C03b — Restore del clon (hit) + re-link

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: C03 (caché de especialización: restaurar el clon y re-vincular).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | En `ensureTypeSpecialization`, ante miss en memoria, restaurar el clon desde la caché C3 y re-vincular (reserve + commit) saltando la materialización. |
| Naturaleza | Añadido (sin cambio de conducta con caché deshabilitada; el hit salta el clonado). |
| Comportamiento idéntico | Sin caché configurada, igual que antes. |
| Revisión de referencia | `development` en `e561d3fba`. |
| Archivos | `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | (restore C3 en `ensureTypeSpecialization`). |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- El nombre especializado se calcula antes del dedup; ante miss en memoria, se consulta la caché y, si
  hay hit, se restaura el clon (`deserializeAstNodeTree`) y se re-vincula vía
  `reserveTypeSpecialization` + `commitTypeSpecialization`.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
