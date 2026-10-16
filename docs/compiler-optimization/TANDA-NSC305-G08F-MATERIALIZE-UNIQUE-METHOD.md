# Tanda NSC305-G08f — Extraer `materializeUniqueMethod`

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: G08 (modularización de la llamada genérica: materializar método único).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Extraer la normalización + materialización del método único de `processGenericCall` a `materializeUniqueMethod`. |
| Naturaleza | Extracción pura (sin cambio de conducta). |
| Comportamiento idéntico | Normalización, materialización y reescritura del sitio. |
| Revisión de referencia | `development` en `f8f1429f`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`, `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `materializeUniqueMethod`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `materializeUniqueMethod` normaliza + materializa el método único + reescribe.
- `processGenericCall` delega ese camino; queda como dispatcher de diagnóstico/aridad + materialización.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
