# Tanda NSC305-G08e — Extraer `materializeSharedOverloads`

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: G08 (modularización de la llamada genérica: materializar sobrecargas compartidas).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Extraer la materialización de sobrecargas compartidas (camino "ID 132") de `processGenericCall` a `materializeSharedOverloads`. |
| Naturaleza | Extracción pura (sin cambio de conducta). |
| Comportamiento idéntico | Materialización de sobrecargas compartidas, grupo de overload y reescritura. |
| Revisión de referencia | `development` en `4f12a0e2`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`, `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `materializeSharedOverloads`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `materializeSharedOverloads` contiene el bucle de sobrecargas compartidas (nombre común, grupo de
  overload, ensureMethodSpecialization) + reescritura del sitio.
- `processGenericCall` delega ese camino.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
