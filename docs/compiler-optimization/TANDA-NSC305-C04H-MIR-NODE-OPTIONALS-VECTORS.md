# Tanda NSC305-C04h — Completar la serialización del nodo MIR (optionals + vectors)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: C04 (caché MIR: optionals/vectors del payload relocatable).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Serializar los optionals (ordinals + enums opcionales + bitOffset/bitWidth) y los vectores (cleanupRegionOrdinals/switchLabels/collectionSegments/argumentParameterOrdinals). |
| Naturaleza | Añadido (sin cambio de conducta; completa el payload). |
| Comportamiento idéntico | Sin cambio. |
| Revisión de referencia | `development` en `03d3c0403`. |
| Archivos | `src/Frontend/ProjectOopMirModel.cpp`. |
| Símbolos | `appendMirSizeVector`, `appendMirInt64Vector`, `appendMirU32Vector`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `appendMirNodeFields` ahora cubre los optionals (byte de presencia + valor) y los vectores; el
  payload C4 queda completo para el restore (deserializar + re-enlazar, siguiente paso).

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
