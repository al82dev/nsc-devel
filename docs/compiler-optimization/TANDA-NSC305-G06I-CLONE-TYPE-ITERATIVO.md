# Tanda NSC305-G06i — `cloneType` iterativo (recursión de tipos a pila explícita)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: G06 (conversión iterativa del clonador de tipos).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Convertir la recursión de return/error/argumentos de `cloneTypeGeneric` a un recorrido iterativo con pila explícita. |
| Naturaleza | Refactor no funcional (preorden conservado; proyección/pack/sustitución/tupla/pack-expansión delegadas a `cloneType`, acotadas). |
| Comportamiento idéntico | Clon de tipos, tuplas, pack-expansión y modificadores. |
| Revisión de referencia | `development` en `ad9e96ca`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`, `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `cloneTypeGeneric` (+`modifierLimit`), `cloneTypeGenericShallow`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | Recursión de tipos genéricos eliminada; 544/544; expediente + commit local aislado. |

## 2. Cambio

- `cloneTypeGeneric` clona el nodo con `cloneTypeGenericShallow` y recorre return/error/argumentos con
  una pila `TypeWork`. Los tipos genéricos simples se clonan aquí y se empujan sus sub-tipos; los tipos
  especiales (proyección/pack/sustitución/tupla/pack-expansión) delegan a `cloneType` (acotados/raros).
- Los modificadores simples se aplican en la pila (sin el manejo de identidad de la sustitución).

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.

## 4. Resultado de G06

- `cloneNode` y `cloneType` ahora son iterativos. La auto-recursión restante queda en los casos raros
  (proyección asociada, pack, sustitución), todos acotados por el guard de profundidad.
