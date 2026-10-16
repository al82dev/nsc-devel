# Tanda NSC305-G05a — Traversal iterativo de `collectTemplates`

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: G05 (eliminación de recursión en el catálogo; primer paso: `collectTemplates`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Convertir `collectTemplates` de recursivo a iterativo (pila explícita), conservando el preorden y el sombreado de owner/namespace. |
| Naturaleza | Refactor no funcional (elimina recursión; sin cambio de orden ni resultado). |
| Comportamiento idéntico | `templateStates_` (orden), `templateRecords_`, ordinals, claves canónicas, `declaredTypeIdentities_`, constantes de enum. |
| Revisión de referencia | `development` en `a58d80f2`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`, `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `ProjectGenericModel::collectTemplates` (firma: strings por valor). |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | Sin recursión en `collectTemplates`; 544/544; expediente + commit local aislado. |

## 2. Cambio

- `collectTemplates` pasa de recursión a un bucle con pila explícita de `CollectFrame`
  (`node`, `parent`, `namespaceName`, `ownerName`, `insideGenericType`). Se empujan los hijos en orden
  inverso para conservar el preorden. La lógica por nodo queda intacta; sólo se sustituye la recursión
  (`return` → `continue` para trait-impl y para `node == nullptr`).
- La firma cambia `const std::string&` → `std::string` por valor (la pila ya copia las cadenas).

## 3. Evidencia

- **HECHO COMPROBADO**: `nsc` compila; `Host.Frontend` **544 PASS / 0 FAIL** (orden determinista
  preservado por la suite).

## 4. Siguiente acción

NSC305-G05b: convertir `discoverTemplateDependencies` / `collectTemplateDependencies` /
`expandAliasesInNode` a iterativos (mismo patrón de pila explícita), y después G06 (clonadores
`cloneNode`/`cloneType` iterativos).
