# Tanda NSC305-G09j — `SpecializationWorker` (estado privado del worker)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: G09 (ondas paralelas: arena privada + demandas hijas del worker).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Añadir `SpecializationWorker` (arena privada + demandas hijas). |
| Naturaleza | Infraestructura (sin cambio de conducta; aún no se consume). |
| Comportamiento idéntico | Sin cambio (estructura nueva). |
| Revisión de referencia | `development` en `1a01ed7d9`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`. |
| Símbolos | `SpecializationWorker`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `SpecializationWorker` posee su arena privada (el clon vive ahí, sin raza en la arena de la unidad) y
  sus demandas hijas.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
