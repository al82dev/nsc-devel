# Tanda NSC305-G09p — Clon paralelo fuera del mutex (speedup fino)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: G09 (ondas paralelas: el clon corre en el worker, el commit en el coordinador).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Dividir el apply en clon (worker, fuera del mutex) + commit/processNode/rewrite (coordinador, serial). |
| Naturaleza | Refactor no funcional (misma salida; paraleliza el clon). |
| Comportamiento idéntico | Misma salida y telemetría serial y paralela. |
| Revisión de referencia | `development` en `5019e1bfe`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`, `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `cloneTypeSpecializationProduct`, `commitTypeSpecializationProduct`, `executeTypeDemand(..., preClonedProduct)`, `nodesCloned`/`typesCloned` atómicos. |
| Pruebas | `Host.Frontend` completo (544 casos), serial y paralelo (2 workers, 3 runs). |
| Cierre | 544/544 en todos; expediente + commit local aislado. |

## 2. Cambio

- `cloneTypeSpecializationProduct` clona en el hilo del worker y captura los errores causales en el
  producto; `commitTypeSpecializationProduct` reporta/enlaza/procesa en el coordinador.
- La onda paralela clona FUERA del mutex y sólo serializa el commit/processNode/rewrite (las
  escrituras compartidas). Los ordinales ya estaban pre-reservados en orden.
- `nodesCloned`/`typesCloned` pasan a atómicos (se escriben desde el worker).

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL** serial y paralelo (2 workers, 3 runs
  consecutivos = determinismo perturbado).
