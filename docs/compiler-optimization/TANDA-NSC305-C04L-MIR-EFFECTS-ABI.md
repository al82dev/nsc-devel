# Tanda NSC305-C04l — Serializar effects + ABI del function

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: C04 (caché MIR: effects + ABI del callable).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Añadir `effects` (bits) y `abi` (returnPassing/hidden/parameterPassing/physicalOrdinals/count) al payload. |
| Naturaleza | Añadido (sin cambio de conducta; aún no se consume). |
| Comportamiento idéntico | Sin cambio. |
| Revisión de referencia | `development` en `86ab33039`. |
| Archivos | `src/Frontend/ProjectOopMirModel.cpp`. |
| Símbolos | `serializeMirFunctionCore`, `restoreMirFunctionCore`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `effects` se serializa como bits y se reconstruye con `EffectSet::with`; `abi` serializa
  returnPassing/hiddenReturnParameterOrdinal/parameterPassing/parameterPhysicalOrdinals/contador.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
