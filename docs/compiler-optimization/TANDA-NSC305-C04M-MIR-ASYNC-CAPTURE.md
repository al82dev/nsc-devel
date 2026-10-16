# Tanda NSC305-C04m — Serializar async frames + capturas del function

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: C04 (caché MIR: tuples async + modos/kinds de captura + corrección de orden ABI).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Añadir asyncFrameLocals/Parameters (tuples) y captureSourceKinds/Modes/OwnsValues; corregir el orden de `physicalParameterCount`. |
| Naturaleza | Añadido + corrección de un bug latente de orden en el codec (no ejercitado aún). |
| Comportamiento idéntico | Sin cambio. |
| Revisión de referencia | `development` en `e1d02a8fc`. |
| Archivos | `src/Frontend/ProjectOopMirModel.cpp`. |
| Símbolos | `serializeMirFunctionCore`, `restoreMirFunctionCore`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- Serializa los tuples async (ordinal/offset/typeKey) y los modos/kinds/owns de captura. Corrige que
  `physicalParameterCount` se leía tras los vectores, mientras se escribía antes (el round-trip del
  payload queda alineado).

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
