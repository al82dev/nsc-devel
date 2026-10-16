# Tanda NSC305-G06d — Extraer `cloneTypeGeneric` (rama no-sustitución)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: G06 (segunda extracción de modularización del clonador de tipos).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Extraer la rama no-sustitución de `cloneType` (clon superficial + return/error + argumentos + tags de origen) a `cloneTypeGeneric`. |
| Naturaleza | Extracción pura (sin cambio de conducta). |
| Comportamiento idéntico | Clon de tipos, return/error, tuplas, argumentos y tags de origen. |
| Revisión de referencia | `development` en `a9d38a52`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`, `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `cloneTypeGeneric`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `cloneTypeGeneric` contiene la rama `else` (no-sustitución) de `cloneType` (~60 líneas).
- `cloneType` ahora delega esa rama a `cloneTypeGeneric` (paso previo a la conversión iterativa).

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.

## 4. Pendiente (misma tanda G06)

- Convertir `cloneType`/`cloneNode` a recorrido iterativo (máquina de frames postorden).
