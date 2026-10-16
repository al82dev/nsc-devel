# Tanda NSC305-C04k — Serializar los campos del function (identidad + flags + vectores)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: C04 (caché MIR: payload del callable — metadata + flags + vectores).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Ampliar `serializeMirFunctionCore`/`restoreMirFunctionCore` con los campos escalares, strings, flags, enums, optional y vectores del function. |
| Naturaleza | Añadido (sin cambio de conducta; aún no se consume). |
| Comportamiento idéntico | Sin cambio. |
| Revisión de referencia | `development` en `6e26c33dd`. |
| Archivos | `src/Frontend/ProjectOopMirModel.cpp`. |
| Símbolos | `serializeMirFunctionCore`, `restoreMirFunctionCore`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- El payload del callable ahora incluye nombres, tipos, flags, enums de política, lambda ordinal,
  tamaños de closure/frame async y los vectores de ordinales locales/capturados. Quedan fuera (capa
  siguiente) `effects` (EffectSet sin reconstructor público por bits), `abi`, `cleanupRegions`,
  `exceptionFlow`, los tuples async y los modos/kinds de captura.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
