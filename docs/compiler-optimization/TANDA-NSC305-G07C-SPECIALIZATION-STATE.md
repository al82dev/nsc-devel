# Tanda NSC305-G07c — `SpecializationState` (ciclo de vida explícito del slot)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: G07 (tercer paso: estado explícito del slot de especialización).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Añadir `SpecializationState` (Unseen/Queued/Building/Ready/Failed) y un campo `state` a `GenericSpecializationRecord`, marcando las transiciones reales del flujo secuencial. |
| Naturaleza | Estado explícito (sin cambio de conducta; el flujo sigue igual). |
| Comportamiento idéntico | Materialización, rollback y dedup por clave. |
| Revisión de referencia | `development` en `7782a191`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`, `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `SpecializationState`, `GenericSpecializationRecord::state`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `SpecializationState`: `Unseen` (sin slot), `Queued` (planner G09), `Building` (materializando),
  `Ready` (producto adjuntado), `Failed` (error reutilizable).
- El flujo secuencial marca `Building` tras `push_back` en `ensure*Specialization` y `Ready` tras
  `specializationBySyntax_`; los registros precargados (seed) y las instancias lambda nacen `Ready`.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.

## 4. Nota

`Queued`/`Failed` se usarán en el planner determinista (G09); en el flujo secuencial actual no hay
cola ni reutilización de errores (el slot fallido se retira con `pop_back`, conducta sin cambios).
