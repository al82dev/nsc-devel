# Tanda NSC305-G09i — `executeTypeSpecializationProduct` (worker produce el producto)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: G09 (ondas paralelas: el worker devuelve el producto privado).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Extraer `executeTypeSpecializationProduct` (clonar + processNode) que devuelve el `SpecializationProduct`; `executeTypeSpecialization` queda como wrapper bool. |
| Naturaleza | Refactor puro (sin cambio de conducta). |
| Comportamiento idéntico | Igual salida; el producto aún no se consume en paralelo. |
| Revisión de referencia | `development` en `af6619228`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`, `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `executeTypeSpecializationProduct`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `executeTypeSpecializationProduct` ejecuta (clonar + commit + processNode + store) y devuelve el
  producto (ordinal + clon + éxito); `executeTypeSpecialization` lo envuelve. Base del worker paralelo.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
