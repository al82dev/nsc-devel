# Tanda NSC305-G08l — Activar las dos pasadas + profundidad lógica

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: G08 (activar collect→waves preservando la profundidad de instanciación).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Activar la dos pasadas (processTypeSyntax recolecta, ondas ejecutan) y llevar la profundidad lógica en la demanda para que el InstantiationBudget conserve el límite y `maximumObservedDepth`. |
| Naturaleza | Refactor no funcional (misma salida; orden de materialización por ondas deterministas). |
| Comportamiento idéntico | Mismos nombres/ordinals/por salida; profundidad observada conservada. |
| Revisión de referencia | `development` en `b084c8077`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`, `include/nativesharp/Language/GenericSemantics.h`, `src/Frontend/ProjectGenericModel.cpp`, `src/Language/GenericSemantics.cpp`. |
| Símbolos | `GenericTypeDemand::logicalDepth`, `currentLogicalDepth_`, `InstantiationBudget::enter(identity, depth, error)`. |
| Pruebas | `Host.Frontend` completo (544 casos, incl. TF-FRONT-168 de profundidad). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `processTypeSyntax` ahora recolecta (`collectTypeSpecializationDemand`) y `build()` drena
  `executeTypeDemandWaves` tras las body-type demands.
- `processBodyTypeDemandWave` materializa directamente (resuelve + materializa, sin re-colectar).
- La demanda lleva `logicalDepth` (padre + 1); el scheduler la restaura; `InstantiationBudget::enter`
  acepta la profundidad lógica y la usa para el límite y `maximumObservedDepth`.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL** (TF-FRONT-168 pasa con las dos pasadas activas).
