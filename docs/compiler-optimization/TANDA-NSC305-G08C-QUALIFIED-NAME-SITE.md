# Tanda NSC305-G08c — `makeQualifiedNameSite` (construcción de sitio compartida)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: G08 (tercer paso de modularización: extraer la construcción del sitio de resolución).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Extraer la construcción repetida de `QualifiedNameSite` (splitDotted + enclosingTypeSpellings + aliases + imports) a `makeQualifiedNameSite`, que devuelve un `QualifiedNameSiteContext` dueño de los vectores. |
| Naturaleza | Extracción pura (sin cambio de conducta). |
| Comportamiento idéntico | Sitio de resolución (namespace segments, enclosing types, aliases, imports). |
| Revisión de referencia | `development` en `680475bb`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`, `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `makeQualifiedNameSite`, `QualifiedNameSiteContext`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `QualifiedNameSiteContext` posee los vectores; `makeQualifiedNameSite` los construye y asigna los
  spans del sitio (C++17 elisión garantiza la validez de los spans).
- 3 sitios (`inferGenericCall`, `processGenericCall`, `collectCallDependency`) usan el helper.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.

## 4. Pendiente (misma tanda G08)

- `processGenericCall → GenericCallDemandAnalyzer` y separación `DemandCollector`/`RewriteApplier`.
