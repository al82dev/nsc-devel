# Tanda NSC305-G06c — Extraer `foldValuePack` del clonador

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: G06 (paso de modularización del clonador: extraer el plegado de packs de valor).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Extraer el plegado de packs de valor (packcount/packat/packsum/packproduct/packall/packany) de `cloneNode` a un helper `foldValuePack`. |
| Naturaleza | Extracción pura (sin cambio de conducta). |
| Comportamiento idéntico | Plegado, errores de pack y fallthrough al camino genérico. |
| Revisión de referencia | `development` en `e4c2709b`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`, `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `foldValuePack`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `foldValuePack` devuelve el nodo plegado, o `nullptr` cuando el nodo no es un plegado de pack
  (para que `cloneNode` caiga al camino genérico). El bloque (~110 líneas) sale de `cloneNode`.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.

## 4. Pendiente (misma tanda G06)

- Convertir `cloneType`/`cloneNode` (y la proyección asociada) a recorrido iterativo. Este commit es
  la primera extracción de modularización; la conversión iterativa sigue.
