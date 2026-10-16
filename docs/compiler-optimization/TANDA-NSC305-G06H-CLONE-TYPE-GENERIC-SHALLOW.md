# Tanda NSC305-G06h — Extraer `cloneTypeGenericShallow` (clon superficial de tipo)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: G06 (separar la parte superficial del clon genérico de tipo).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Extraer la parte superficial (creación + flags de función + tags de origen) de `cloneTypeGeneric` a `cloneTypeGenericShallow`, dejando fuera return/error/argumentos. |
| Naturaleza | Extracción pura (sin cambio de conducta). |
| Comportamiento idéntico | Clon de tipos. |
| Revisión de referencia | `development` en `8c867b72`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`, `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `cloneTypeGenericShallow`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `cloneTypeGenericShallow` crea el resultado y copia flags de función + tags de origen.
- `cloneTypeGeneric` lo usa y conserva la clonación de return/error/argumentos (prerequisito para la
  pila explícita).

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
