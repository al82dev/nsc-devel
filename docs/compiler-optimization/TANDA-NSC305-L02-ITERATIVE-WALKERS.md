# Tanda NSC305-L02 — Walkers iterativos de partición

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L02 (eliminar recursión de partición: nodeCost + collectNode + destructores).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Convertir `nodeCost`, `collectNode` y el paseo de destructores a iterativos. |
| Naturaleza | Refactor no funcional (mismas particiones y salida). |
| Comportamiento idéntico | Mismas particiones (legacy) y misma salida. |
| Revisión de referencia | `development` en `97f6c3d81`. |
| Archivos | `src/LLVM/ProjectOopCodePartition.cpp`. |
| Símbolos | `nodeCost`, `collectNode`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `nodeCost` recorre el árbol con pila explícita; `collectNode` también, con el paseo de dependencias de
  destructores en pila (un `seen` set por nodo sustituye al `visiting` recursivo). Las dependencias
  quedan en un `unordered_set`, de modo que el resultado es idéntico al legacy.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
