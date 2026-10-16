# Tanda NSC305-C01m — Restore WARM (hit) de la caché C1

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: C01 (integración: restore warm con conteo de hit).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Añadir `restoreTemplateRecords`: consulta la caché C1 por cada record y cuenta un hit cuando la metada redondea. |
| Naturaleza | Añadido (sin cambio de conducta con caché deshabilitada). |
| Comportamiento idéntico | Sin caché configurada, no hace nada. |
| Revisión de referencia | `development` en `2a8921dd`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`, `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `restoreTemplateRecords`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `restoreTemplateRecords` mira la caché, deserializa la metada y cuenta el hit (telemetría de
  especialización reutilizada). El subárbol AST se sigue caminando; el splice AST es una capa posterior.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
