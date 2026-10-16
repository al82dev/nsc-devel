# Tanda NSC305-G09f — `InstantiationContext` (contexto poseído por la demanda)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: G09 (§6.4: cada demanda posee su contexto de instanciación).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Añadir `InstantiationContext` (proyecto/namespace/range/overload/packs/proyecciones) al record de especialización, para que la demanda posea su contexto (base del worker privado). |
| Naturaleza | Refactor no funcional (se guarda el contexto que ya se usaba). |
| Comportamiento idéntico | Reserva de especialización (mismos datos almacenados). |
| Revisión de referencia | `development` en `95d2ac2b`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`, `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `InstantiationContext`, `GenericSpecializationRecord::instantiationContext`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `InstantiationContext` agrupa el sitio de la petición + packs + proyecciones.
- `reserveTypeSpecialization` y la siembra lo guardan en el record (los workers lo usarán sin stack C++).

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
