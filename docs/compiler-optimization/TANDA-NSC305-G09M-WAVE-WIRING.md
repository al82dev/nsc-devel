# Tanda NSC305-G09m — Cablear la onda (reserve-all + execute-all)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: G09 (ondas paralelas: pre-reserva determinista + apply por demanda).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Cablear `executeTypeDemandWaves` para pre-reservar todos los ordinales en orden y luego aplicar cada demanda; extraer `rewriteSpecializedSyntax`. |
| Naturaleza | Refactor no funcional (misma salida; estructura worker/committer). |
| Comportamiento idéntico | Mismas especializaciones y telemetría. |
| Revisión de referencia | `development` en `3216040f6`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`, `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `rewriteSpecializedSyntax`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `executeTypeDemandWaves` ahora pre-reserva todos los ordinales en el orden ordenado (antes de
  ejecutar) y luego aplica cada demanda por `executeTypeDemand`; los hits reusan el nombre publicado.
- `rewriteSpecializedSyntax` unifica el rewrite del nodo (identidad canónica + nombre + clear).
- `SpecializationWaveTimer` se mantiene en el apply (materialización real), no en el hit.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
