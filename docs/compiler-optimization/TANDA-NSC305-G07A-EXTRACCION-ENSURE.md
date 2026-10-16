# Tanda NSC305-G07a — Extraer pasos compartidos de `ensure*Specialization`

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: G07 (primer paso de modularización: helpers puros compartidos, sin cambio de conducta).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Extraer la computación de clave y la preparación de sustituciones/packs (idénticas en `ensureTypeSpecialization` y `ensureMethodSpecialization`) a helpers compartidos. |
| Naturaleza | Extracción pura (sin cambio de conducta). |
| Comportamiento idéntico | Clave de especialización, spellings/identities, sustituciones y packs. |
| Revisión de referencia | `development` en `253d086f`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`, `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `computeSpecializationKey`, `prepareSpecializationSubstitutions`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `computeSpecializationKey`: construye spellings/identities y la clave canónica `<...>`.
- `prepareSpecializationSubstitutions`: arma sustituciones fijas + pack y limpia `activePackSubstitutions_`.
- Ambas `ensure*Specialization` usan los helpers (elimina ~45 líneas duplicadas).

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.

## 4. Pendiente (misma tanda G07)

- Contexto de especialización (`SpecializationContext`) para salvar/restaurar los 10 campos.
- `SpecializationSlot` + `SpecializationPlanner::request` + commit/abort (base para G09).
