# Tanda NSC305-G05b — Traversals de dependencias iterativos

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: G05 (segundo paso: `discoverTemplateDependencies` y `collectTypeDependency`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Convertir `discoverTemplateDependencies` (recorrido preorden de sintaxis) y `collectTypeDependency` (recorrido del árbol de argumentos de tipo) de recursivos a iterativos. |
| Naturaleza | Refactor no funcional (elimina recursión; orden de resultado irrelevante porque se ordena/dedup después). |
| Comportamiento idéntico | Dependencias, `unresolvedDependencies`, `externalTypeReferences` (ordenadas y dedup después). |
| Revisión de referencia | `development` en `ac72709c`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`, `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `discoverTemplateDependencies` (firma por valor), `collectTypeDependency`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | Sin recursión en esos dos símbolos; 544/544; expediente + commit local aislado. |

## 2. Cambio

- `discoverTemplateDependencies`: pila explícita de `DependencyFrame` (node/namespace/owner/scope);
  preorden conservado (hijos en orden inverso); `return` de plantilla raíz → `continue`.
- `collectTypeDependency`: pila de `TypeSyntax*`; los argumentos se empujan siempre y el nodo se
  procesa después. El orden de visita no afecta el resultado porque `build()` ordena y deduplica.

## 3. Evidencia

- **HECHO COMPROBADO**: `nsc` compila; `Host.Frontend` **544 PASS / 0 FAIL**.

## 4. Siguiente acción

NSC305-G05c: convertir `collectTemplateDependencies` (recorrido de cuerpos) y `expandAliasesInNode`
a iterativos; luego G06 (clonadores iterativos).
