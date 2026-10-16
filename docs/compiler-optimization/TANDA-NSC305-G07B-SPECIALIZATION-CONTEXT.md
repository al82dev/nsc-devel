# Tanda NSC305-G07b — `SpecializationContext` (snapshot del contexto de instanciación)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: G07 (segundo paso: agrupar los 10 campos de contexto salvar/restaurar).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Agrupar los 10 campos mutables de "instanciación actual" que `ensure*Specialization` salva/restaura en un struct `SpecializationContext` con `save`/`restore`. |
| Naturaleza | Extracción pura (agrupación; sin cambio de conducta). |
| Comportamiento idéntico | Orden y alcance de la restauración del contexto (los accesos se renombran 1:1). |
| Revisión de referencia | `development` en `f0907c2b`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`, `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `SpecializationContext` (`save`/`restore`). |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `SpecializationContext` (struct anidado) copia/restaura los 10 campos; `save`/`restore` acceden a los
  privados del modelo (struct anidado = miembro).
- Ambas `ensure*Specialization` usan `SpecializationContext callerContext; callerContext.save(*this);`
  y referencian `callerContext.*` en la restauración (renombrado 1:1, sin alterar orden/alcance).

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.

## 4. Pendiente (misma tanda G07)

- `SpecializationSlot` (Unseen→Queued→Building→Ready/Failed) + `SpecializationPlanner::request`.
