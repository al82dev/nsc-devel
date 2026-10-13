# NS304-T03-F08 — Diagnósticos de casos faltantes, redundancia, escape y move inválido

- **Estado**: EVIDENCIA TÉCNICA ADELANTADA PARCIAL; cierre secuencial pendiente.
- **Rama**: development

Este expediente no incrementa el 25/147 vigente; F04 continúa siendo el siguiente ítem secuencial.

## Auditoría (línea base)

| Diagnóstico | Estado | Evidencia |
|---|---|---|
| Casos faltantes (variante) | ✅ | `analyzeClosedMatch` + reporte "missing variant case" (ProjectMethodBodyModel.cpp:4383/8591) |
| Casos faltantes (longitud de list) | ✅ | F05 `analyzeListMatch` + "missing list length" |
| Escape de rest view | ✅ | `TL-LANG-365` negativo exacto + `TL-LANG-309` guard local no escapante |
| Redundancia (brazo unreachable) variante | ✅ | `analyzeClosedMatch.unreachableArms` → reporte 4374 |
| Redundancia (list) | ❌ | `analyzeListMatch` no calcula `unreachableArms` |
| Move inválido | parcial | validación `orPatternBindingsCompatible` (modo) + binding mode validation; falta "move de tipo no moveable" específico |

## Gaps F08

1. `analyzeListMatch` debe calcular `unreachableArms` (brazos de longitud ya cubierta) y el Sema
   debe reportarlos.
2. Diagnóstico de "move inválido" (p.ej. `move` sobre un campo de un `copy borrowed struct`
   no moveable) si aún no está.

## Prueba puntual

`TL-LANG-315`: brazo de list redundante (p.ej. `[var x]` después de `[.. var r]`) debe
diagnosticar "unreachable"; move inválido debe diagnosticar.
