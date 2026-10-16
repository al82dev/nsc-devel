# Tanda NSC305-G05c — Traversals de cuerpos y alias iterativos

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: G05 (cierre: `collectTemplateDependencies` y `expandAliasesInNode`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Convertir `collectTemplateDependencies` (recorrido de cuerpos de template) y `expandAliasesInNode` (recorrido de expansión de alias) de recursivos a iterativos. |
| Naturaleza | Refactor no funcional (elimina recursión; preorden conservado). |
| Comportamiento idéntico | Dependencias, aliases expandidos, orden de visita. |
| Revisión de referencia | `development` en `da0aee3c`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`, `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `collectTemplateDependencies`, `expandAliasesInNode` (firmas por valor). |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | Sin recursión en esos símbolos; 544/544; expediente + commit local aislado. |

## 2. Cambio

- `collectTemplateDependencies`: pila explícita `BodyFrame`; preorden con first/second/third y children
  empujados en orden inverso; guardas `nullptr`/plantilla anidada → `continue`.
- `expandAliasesInNode`: pila explícita `AliasFrame`; `ok` acumulado (`nodeOk` por nodo → `ok` global);
  alias-expansion por nodo intacta; children/first/second/third en orden inverso.

## 3. Evidencia

- **HECHO COMPROBADO**: `nsc` compila; `Host.Frontend` **544 PASS / 0 FAIL**.

## 4. Siguiente acción

NSC305-G06: convertir los clonadores `cloneNode`/`cloneType` a iterativos (elimina recursión en la ruta
de materialización).
