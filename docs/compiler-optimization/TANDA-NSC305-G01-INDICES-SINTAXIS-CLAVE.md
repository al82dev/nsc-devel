# Tanda NSC305-G01 — Índices por sintaxis y clave canónica

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: G01 (complejidad algorítmica de dos sitios de `ProjectGenericModel`; sin cambio de reglas de resolución).

## 1. SCOPE LOCK de la tanda

| Campo | Valor |
|---|---|
| Objetivo | Sustituir dos barridos lineales sobre `templateStates_` por índices O(1): `stateForSyntax` (`find_if` por `state.syntax`) y la siembra de especializaciones importadas (`find_if` por `canonicalKey`). No se tocan reglas de resolución, visibilidad, ordinals ni names. |
| Naturaleza | Refactor de complejidad no funcional (O(T)→O(1) por consulta). |
| Comportamiento idéntico | `templateRecords_`, ordinals, `specializations_`, dependencias, diagnósticos, orden. |
| Revisión de referencia | `development` en `13b59990`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`, `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `ProjectGenericModel::stateForSyntax`, bloque de siembra de `build()`, nuevos miembros `stateBySyntax_`/`stateByCanonicalKey_`. |
| Pruebas | Componente `Host.Frontend` completo (544 casos) sin modificaciones. |
| Prohibido | Cambiar reglas de resolución, ordinals, diagnóstico, o tocar otros `find_if` (fuera de G01). |
| Cierre | Índices construidos y usados; `find_if` de esos dos sitios eliminado; 544/544; expediente + commit local aislado. |

## 2. Cambio

- `ProjectGenericModel.h`: dos mapas `stateBySyntax_` (`AstNode* → TemplateState*`) y
  `stateByCanonicalKey_` (`canonicalKey → TemplateState*`).
- `build()`: los construye tras `collectTemplates` y tras la desambiguación de claves canónicas
  (cuando el vector ya es estable), con `emplace` (el primer estado gana, como el `find_if` previo).
- `stateForSyntax`: `find_if` → `stateBySyntax_.find` (O(1)).
- Siembra importada: `find_if` → `stateByCanonicalKey_.find` (O(1)).

## 3. Evidencia

- **HECHO COMPROBADO**: `nsc` compila; `Host.Frontend` **544 PASS / 0 FAIL** (misma conducta pública).
- **HECHO COMPROBADO**: grep confirma que no queda `find_if(templateStates_` en esos dos sitios; sólo
  los índices (`stateBySyntax_`/`stateByCanonicalKey_`).
- **INFERENCIA**: la complejidad de esos dos sitios pasa de O(T) por consulta a O(1) promedio; el
  efecto de pared es asintótico y no se mide aquí (corpus/runner P03 quedan listos para medirlo cuando
  la GUI ejecute la validación general).

## 4. Siguiente acción

NSC305-G02: índices de templates de tipo (`resolveTypeTemplate` devuelve triestado
`Missing/Unique/Ambiguous`; cubetas por módulo/nombre calificado y por nombre visible/aridad).
