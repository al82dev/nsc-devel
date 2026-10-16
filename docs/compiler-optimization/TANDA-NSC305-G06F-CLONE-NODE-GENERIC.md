# Tanda NSC305-G06f — Extraer `cloneNodeGeneric` (clon genérico sin recursión de hijos)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: G06 (cuarta extracción de modularización: aislar el clon genérico del clonador de nodos).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Extraer el clon genérico de un nodo (campos, modificadores, parámetros genéricos, constraints, tipo/throws) a `cloneNodeGeneric`, dejando fuera la recursión de hijos (first/second/third/children). |
| Naturaleza | Extracción pura (sin cambio de conducta). |
| Comportamiento idéntico | Clon de nodos. |
| Revisión de referencia | `development` en `78e39b0a`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`, `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `cloneNodeGeneric`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `cloneNodeGeneric` contiene el clon genérico (sin recursión de hijos). `cloneNode` delega esa parte.
- Ahora la recursión de `cloneNode` queda reducida a los casos especiales (member/index/call) y el
  bucle de hijos, listo para convertir a pila explícita.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
