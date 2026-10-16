# Tanda NSC305-C03c — Restore C3 resuelve las demandas hijas

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: C03 (caché de especialización: el hit restaurado resuelve las demandas hijas vía cache hits).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Tras restaurar + re-vincular el clon, correr `processNode` para resolver las demandas hijas (que a su vez hacen hit C3) y restaurar el contexto del caller. |
| Naturaleza | Corrección del restore C3 (sin cambio con caché deshabilitada). |
| Comportamiento idéntico | Sin caché configurada, igual que antes. |
| Revisión de referencia | `development` en `1b004fb25`. |
| Archivos | `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | (restore C3 en `ensureTypeSpecialization`). |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- El hit C3 restaura el clon, lo re-vincula y ejecuta `processNode` (caminata + hits de caché de las
  hijas, sin re-clonar) y restaura el contexto de instanciación del caller.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
