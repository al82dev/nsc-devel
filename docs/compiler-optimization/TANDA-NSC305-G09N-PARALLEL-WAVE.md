# Tanda NSC305-G09n — Onda paralela (workers + pool)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: G09 (ondas paralelas: ejecutar la onda en workers deterministas).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Ejecutar la onda en workers (`std::async` + mutex) cuando `parallelismWorkers_ > 1`; añadir `setParallelismWorkers`. |
| Naturaleza | Refactor no funcional (misma salida; orden por ordinal pre-reservado). |
| Comportamiento idéntico | Con 1 worker (por defecto), mismo camino serial. Con N workers, mismos ordinales y salida. |
| Revisión de referencia | `development` en `853f6a127`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`, `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `setParallelismWorkers`, `parallelismWorkers_`. |
| Pruebas | `Host.Frontend` completo (544 casos), en serial y en paralelo (2 workers). |
| Cierre | 544/544 en ambos; expediente + commit local aislado. |

## 2. Cambio

- `executeTypeDemandWaves` ejecuta el apply de la onda en `std::async` (una tarea por demanda) con un
  mutex sobre el modelo compartido cuando `parallelismWorkers_ > 1` y la onda tiene más de una demanda.
  Los ordinales ya estaban pre-reservados en orden, así la salida es determinista.
- `setParallelismWorkers` configura el número de workers (0 se normaliza a 1).

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL** en serial (1 worker) y en paralelo
  (2 workers, verificación de la rama `std::async`).
