# Tanda NSC305-C04c — `MirFunctionKey` codec

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: C04 (caché MIR: codec de los campos clave del callable).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Añadir `MirFunctionKey` + `serializeMirFunctionKey`/`restoreMirFunctionKey` (identidad observable del callable). |
| Naturaleza | Añadido (sin cambio de conducta; aún no se consume). |
| Comportamiento idéntico | Sin cambio (codec nuevo). |
| Revisión de referencia | `development` en `db7b6457e`. |
| Archivos | `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `MirFunctionKey`, `serializeMirFunctionKey`, `restoreMirFunctionKey`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `MirFunctionKey` agrupa mangledName/genericSpecializationKey/return/error/parámetros; codec binario
  sin punteros. El flujo de nodos MIR (implementation fingerprint) es la siguiente capa.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
