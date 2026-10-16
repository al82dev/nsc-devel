# Tanda NSC305-C04j — `serializeMirFunctionCore` (identidad + cuerpo del callable)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: C04 (caché MIR: payload del callable — identidad + árbol de nodos).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Añadir `serializeMirFunctionCore`/`restoreMirFunctionCore` (mangledName + genericSpecializationKey + return/error/param typeKeys + body). |
| Naturaleza | Añadido (sin cambio de conducta; aún no se consume). |
| Comportamiento idéntico | Sin cambio. |
| Revisión de referencia | `development` en `515fa842d`. |
| Archivos | `src/Frontend/ProjectOopMirModel.cpp`. |
| Símbolos | `serializeMirFunctionCore`, `restoreMirFunctionCore`, `MirByteReader::remaining`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- El payload del callable une la identidad observable (mangledName + specialization + tipos) con el
  árbol de nodos. Los campos restantes del function (ABI/effects/closure/exception flow) son la
  siguiente capa de serialización.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
