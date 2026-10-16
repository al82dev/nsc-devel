# Tanda NSC305-G08j — `GenericTypeDemand` (estructura de demanda de tipo)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: G08 (dos pasadas: estructura de la demanda que recolecta el collector).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Añadir `GenericTypeDemand` (state/arguments/target/key/name/identities/context) y la cola `typeSpecializationDemands_` como base de la dos pasadas collect→apply. |
| Naturaleza | Infraestructura (sin cambio de conducta; aún no se consume). |
| Comportamiento idéntico | Sin cambio (estructura nueva). |
| Revisión de referencia | `development` en `7fa30034`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`. |
| Símbolos | `GenericTypeDemand`, `typeSpecializationDemands_`. |
| Pruebas | `Host.Frontend` completo (544 casos, sin regresión). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `GenericTypeDemand` describe una demanda de especialización de tipo (estado, argumentos, target a
  reescribir, clave, nombre, identidades, contexto).
- `typeSpecializationDemands_` será la cola recolectada por discovery y drenada por el executor.

## 3. Evidencia

- **HECHO COMPROBADO**: compila y `Host.Frontend` **544 PASS / 0 FAIL**.
