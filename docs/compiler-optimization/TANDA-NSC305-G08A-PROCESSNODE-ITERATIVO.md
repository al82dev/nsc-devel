# Tanda NSC305-G08a — `processNode` iterativo (pila explícita)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: G08 (primer paso: eliminar la recursión del traversal central, sin separar aún discovery/rewrite).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Convertir `processNode` (el traversal central que mezcla scope/inferencia/materialización) de recursivo a iterativo con pila explícita. |
| Naturaleza | Refactor no funcional (preorden conservado; la acumulación secuencial de locals se preserva). |
| Comportamiento idéntico | Orden de visita, sombreado de locals, materialización y diagnósticos. |
| Revisión de referencia | `development` en `08f09132`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`, `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `processNode` (firma por valor + `Frame` + pila). |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | Sin recursión en `processNode`; 544/544; expediente + commit local aislado. |

## 2. Cambio

- Firma por valor (`namespaceName`/`ownerName`/`scope`) y bucle `while` con `Frame{node, namespace,
  owner, scope, insideTemplate}`.
- `return` temprano (nulo, alias directive, nodo plantilla) → `continue`.
- La acumulación secuencial de locals se hace en un pre-pase que calcula el snapshot de scope de cada
  hijo (`childScopes`) antes de empujar los hijos en orden inverso (pop en orden fuente).

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.

## 4. Pendiente (misma tanda G08)

- Separar discovery (`GenericDemandCollector`) de rewrite (`RewriteApplier`) — el paso algorítmico
  posterior; este commit es sólo el cambio mecánico (iterativo).
