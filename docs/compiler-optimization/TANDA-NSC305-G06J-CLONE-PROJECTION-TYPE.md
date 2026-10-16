# Tanda NSC305-G06j — Extraer `cloneProjectionType` (proyección asociada)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: G06 (cierre: extraer la normalización de proyección asociada).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Extraer la normalización de proyección asociada (`T.Item`) de `cloneType` a `cloneProjectionType` (bucle ya acotado por dot-count + detección de ciclo). |
| Naturaleza | Extracción pura (sin cambio de conducta). |
| Comportamiento idéntico | Normalización de proyección, ciclo y errores. |
| Revisión de referencia | `development` en `4adc53e2`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`, `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `cloneProjectionType`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `cloneProjectionType` contiene el bucle de proyección (candidatos + asociados + ciclo) y devuelve
  `nullptr` cuando la fuente no es proyección.
- `cloneType` delega esa rama.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.

## 4. Resultado de G06

- Clonadores `cloneNode` y `cloneType` iterativos; proyección asociada aislada en `cloneProjectionType`
  (bucle + detección de ciclo, acotada). G06 queda cerrado.
