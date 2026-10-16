# Tanda NSC305-G08b — `processTypeSyntax` devuelve la clave de especialización

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: G08 (segundo paso: exponer el `SpecializationId` que el planner demand-driven consumirá).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Añadir un parámetro de salida `outSpecializationKey` a `processTypeSyntax` (default `nullptr`) que recibe la clave canónica de la especialización materializada. |
| Naturaleza | Preparatorio (sin cambio de conducta; el parámetro aún no se consume). |
| Comportamiento idéntico | Materialización, reescritura y diagnósticos. |
| Revisión de referencia | `development` en `91b824fb`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`, `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `processTypeSyntax` (+`outSpecializationKey`). |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `processTypeSyntax` acepta `std::string* outSpecializationKey = nullptr`; en el camino de
  materialización exitosa escribe `specializationKey`. Los llamadores actuales no lo pasan (default).

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.

## 4. Pendiente (misma tanda G08/G09)

- `processGenericCall` → `GenericCallDemandAnalyzer`.
- Separar `processNode` en `GenericDemandCollector` + `RewriteApplier` (usa el `SpecializationId`).
