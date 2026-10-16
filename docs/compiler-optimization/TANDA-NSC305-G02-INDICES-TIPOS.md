# Tanda NSC305-G02 — Índices de templates de tipo

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: G02 (complejidad de `resolveTypeTemplate`; sin cambio de semántica de resolución).

## 1. SCOPE LOCK de la tanda

| Campo | Valor |
|---|---|
| Objetivo | Sustituir los dos barridos de `templateStates_` dentro de `resolveTypeTemplate` por cubetas de candidatos de tipo (sólo tipos, no métodos): `typesByModuleQualified_` (módulo + nombre calificado) y `typesByVisibleName_` (nombre terminal). |
| Naturaleza | Refactor de complejidad no funcional (O(T)→O(1+C) por consulta, C = homónimos). |
| Comportamiento idéntico | Resultado de resolución (Missing/Unique/Ambiguous vía `nullptr`), visibilidad, aridad/packs, aliases, nested types, cross-module, diagnósticos, ordinals. |
| Revisión de referencia | `development` en `422e4d9c`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`, `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `ProjectGenericModel::resolveTypeTemplate`, nuevos miembros `typesByModuleQualified_`/`typesByVisibleName_`, bloque de construcción en `build()`. |
| Pruebas | `Host.Frontend` completo (544 casos) sin modificaciones. |
| Prohibido | Cambiar la regla "primer match gana", ambigüedad, visibilidad o tocar resolución de métodos. |
| Cierre | Cubetas construidas y usadas; barridos de `templateStates_` en `resolveTypeTemplate` eliminados; 544/544; expediente + commit local aislado. |

## 2. Cambio

- `ProjectGenericModel.h`: dos cubetas `typesByModuleQualified_` (clave `module::qualifiedName`) y
  `typesByVisibleName_` (clave `record.name`), ambas sólo con templates de tipo y en orden fuente.
- `build()`: las construye en el mismo pase que los índices G01.
- `resolveTypeTemplate`: el camino módulo-calificado consulta `typesByModuleQualified_` y el camino
  visible consulta `typesByVisibleName_` (por nombre terminal, para nombres simples y punteados); la
  lógica de "primer match gana"/ambigüedad y el filtro de visibilidad/aridad se conservan. La
  telemetría `noteResolutionQuery` ahora cuenta el tamaño de la cubeta (homónimos), no el catálogo.

## 3. Evidencia

- **HECHO COMPROBADO**: `nsc` compila; `Host.Frontend` **544 PASS / 0 FAIL**.
- **HECHO COMPROBADO**: el cuerpo de `resolveTypeTemplate` ya no itera `templateStates_`; sólo
  itera la cubeta relevante (grep/lectura).
- **INFERENCIA**: `candidatos examinados` pasa de depender del total T a depender de los homónimos;
  el efecto de pared se medirá sobre el corpus P03 cuando la GUI ejecute la validación general.

## 4. Siguiente acción

NSC305-G03: resolución única e indexada de métodos (`GenericMethodResolution`; `processGenericCall`,
dependencia y diagnóstico comparten un resultado; índices por owner/name/arity/argc; `templatesNamedBy`
deja de barrer el catálogo por rung).
