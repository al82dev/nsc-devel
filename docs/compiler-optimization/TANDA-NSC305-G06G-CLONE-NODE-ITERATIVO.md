# Tanda NSC305-G06g — `cloneNode` iterativo (recursión de hijos a pila explícita)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: G06 (conversión iterativa del clonador de nodos).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Convertir la recursión de hijos de `cloneNode` (first/second/third/children) a un recorrido iterativo con pila explícita. |
| Naturaleza | Refactor no funcional (preorden conservado; el orden de attach coincide). |
| Comportamiento idéntico | Clon de nodos y tipos; orden de hijos. |
| Revisión de referencia | `development` en `2f5385ac`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`, `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `tryCloneSpecialNode`, `cloneNodeGeneric`; `cloneNode` (pila `ChildWork`). |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | Recursión de hijos eliminada; 544/544; expediente + commit local aislado. |

## 2. Cambio

- `tryCloneSpecialNode` agrupa los casos especiales (constante asociada, fold de pack, sustitución const
  de identificador) y devuelve `nullptr` para el camino genérico.
- `cloneNode` clona el nodo genérico con `cloneNodeGeneric` y recorre el subárbol de hijos con una pila
  `ChildWork` (los hijos genéricos se clonan aquí y se empujan sus propios hijos).

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
- La auto-recursión restante de `cloneNode` queda sólo en `foldValuePack` (índice de pack) y
  `tryCloneSpecialNode` (inicializador de constante asociada), ambas superficiales/acotadas.

## 4. Pendiente (misma tanda G06)

- `cloneType` iterativo (recursión de argumentos/return/error de tipos).
