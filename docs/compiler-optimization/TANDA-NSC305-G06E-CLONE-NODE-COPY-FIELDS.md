# Tanda NSC305-G06e — Extraer `cloneNodeCopyFields` (copia escalar no recursiva)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: G06 (tercera extracción de modularización del clonador de nodos).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Extraer la copia de campos escalares y modificadores de un nodo (parte no recursiva) a `cloneNodeCopyFields`. |
| Naturaleza | Extracción pura (sin cambio de conducta). |
| Comportamiento idéntico | Clon de nodos (texto, flags, modificadores, etc.). |
| Revisión de referencia | `development` en `6d06c2e8`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`, `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `cloneNodeCopyFields`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `cloneNodeCopyFields` crea el resultado y copia los campos escalares + modificadores.
- `cloneNode` delega esa parte (paso previo a la conversión iterativa).

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
