# Tanda NSC305-G09l — `executeTypeDemand` (aplicación de una demanda pre-reservada)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: G09 (ondas paralelas: apply de la demanda pre-reservada).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Añadir `executeTypeDemand` (C3 restore + execute + merge + rewrite) con ordinal/clave/nombre pre-reservados. |
| Naturaleza | Infraestructura (sin cambio de conducta; aún no se consume en la onda). |
| Comportamiento idéntico | Sin cambio (función nueva). |
| Revisión de referencia | `development` en `00144b80b`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`, `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `executeTypeDemand`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `executeTypeDemand` ejecuta la mitad "apply" de una demanda ya pre-reservada: C3 restore + execute +
  merge + rewrite del nodo, usando el ordinal/clave/nombre que `reserveTypeDemand` calculó. Es la
  pieza que la onda paralela ejecutará por worker y fusionará por ordinal.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
