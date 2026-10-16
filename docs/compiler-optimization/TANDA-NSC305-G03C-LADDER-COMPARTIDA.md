# Tanda NSC305-G03c — Una sola ladder por llamada

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: G03 (cierre: compartir la ladder de nombres entre todas las variantes de filtro).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Construir `qualifiedNameLadder` UNA vez por sitio de llamada y reutilizarla en todas las variantes de aridad/cuenta (`templatesNamedBy` + `resolveMethodTemplate`). |
| Naturaleza | Refactor no funcional (misma política de resolución; solo evita reconstruir la ladder). |
| Comportamiento idéntico | Resolución de métodos, ambigüedad y fallback sin dueño. |
| Revisión de referencia | `development` en `576010db`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`, `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `templatesNamedBy` (+`ladder`), `resolveMethodTemplate` (+`ladder`), `resolveGenericCall`, `collectCallDependency`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `templatesNamedBy` acepta `ladder` opcional; si es `nullptr` la construye (conducta previa).
- `resolveMethodTemplate` reenvía `ladder` a `templatesNamedBy`.
- `resolveGenericCall` y `collectCallDependency` construyen la ladder una vez y la pasan a todas las
  variantes (exacta → relajada en argumentos → relajada en aridad).

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.

## 4. Resultado de G03

- Índice por nombre (G03a), reuso del rung ganador (G03a), ladder exacta reutilizada (G03b) y ahora
  ladder compartida en todas las variantes (G03c). G03 queda cerrado.
