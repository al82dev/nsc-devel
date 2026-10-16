# Tanda NSC305-G03 — Resolución indexada de métodos (parte 1: índice por nombre)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: G03 (complejidad de la resolución de métodos; sin cambio de semántica).

## 1. SCOPE LOCK de la tanda

| Campo | Valor |
|---|---|
| Objetivo | Sustituir los barridos de `templateStates_` dentro de `templatesNamedBy` y del camino sin dueño de `resolveMethodTemplate` por una cubeta `methodsByName_` (sólo métodos, orden fuente). |
| Naturaleza | Refactor de complejidad no funcional (O(T)→O(C) por rung, C = métodos homónimos). |
| Comportamiento idéntico | Overloads, receiver-value, primer rung ganador, aliases/imports, owner ambiguity, controlled-specialization, diagnósticos. |
| Revisión de referencia | `development` en `9ab89e5b`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`, `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `ProjectGenericModel::templatesNamedBy`, `ProjectGenericModel::resolveMethodTemplate` (camino sin dueño), nuevo `methodsByName_`, bloque de construcción en `build()`. |
| Pruebas | `Host.Frontend` completo (544 casos) sin modificaciones. |
| Prohibido | Cambiar la regla "primer rung que produce algo gana", ambigüedad de dueño, visibilidad o arity/argc. |
| Cierre | Cubeta construida y usada; barridos de `templateStates_` en esos dos sitios eliminados; 544/544; expediente + commit local aislado. |

## 2. Cambio

- `methodsByName_` (clave `record.name`, sólo templates de método) se construye junto a los índices G01/G02.
- `templatesNamedBy` consulta la cubeta en vez de barrer el catálogo; la lógica de rungs y de owner
  vacío (global) se conserva. `resolveMethodTemplate` usa la misma cubeta en su camino sin dueño.
- `noteResolutionQuery` cuenta el tamaño de la cubeta (homónimos).

## 3. Evidencia

- **HECHO COMPROBADO**: `nsc` compila; `Host.Frontend` **544 PASS / 0 FAIL**.
- **HECHO COMPROBADO**: `templatesNamedBy` y el camino sin dueño ya no iteran `templateStates_`.

## 4. Pendiente explícito (misma tanda, siguiente commit)

El GATE restante de G03 — «una llamada no construye más de una ladder» — no se cierra con este commit:
`processGenericCall` y `collectCallDependency` aún llaman `templatesNamedBy`/`collectMethodTemplates`/
`resolveMethodTemplate` varias veces. Siguiente commit (G03b): introducir `GenericMethodResolution`
(rung ganador, candidatos, disposición, owner) y que ambos consuman un único resultado por sitio.

## 5. Siguiente acción

NSC305-G03b (resultado compartido de resolución de métodos), luego G04 (scopes persistentes).
