# Tanda NSC305-G09o — Estado privado por worker (thread-local)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: G09 (ondas paralelas: estado de clonado/processNode por worker).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Convertir el estado por-clon (pack/associated projections, errores causales, profundidad de clonado) a `thread_local static inline`. |
| Naturaleza | Refactor no funcional (mismo resultado; prepara la ejecución paralela sin raza). |
| Comportamiento idéntico | Misma salida serial y paralela. |
| Revisión de referencia | `development` en `312d2f8a5`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`. |
| Símbolos | `activeAssociatedProjections_`, `associatedProjectionError_`, `associatedProjectionErrorRange_`, `activePackSubstitutions_`, `packMaterializationError_`, `packMaterializationErrorRange_`, `cloneDepth_`. |
| Pruebas | `Host.Frontend` completo (544 casos), serial y paralelo (2 workers). |
| Cierre | 544/544 en ambos; expediente + commit local aislado. |

## 2. Cambio

- El estado que cloneNode/cloneType/processNode mutan por-clon vive ahora en `thread_local static
  inline`, de modo que cada worker (hilo) tiene su propia copia y no compite con un hermano. Es la
  base de "arenas/resultados privados" del diseño G09. La ejecución sigue serializada por el mutex de
  la onda; el speedup fino (sacar el clon fuera del mutex) es el siguiente paso.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL** serial (1 worker) y paralelo (2 workers).
