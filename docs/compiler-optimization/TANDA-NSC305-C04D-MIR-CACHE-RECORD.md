# Tanda NSC305-C04d — `MirCacheRecord` (clave + huellas del MIR por callable)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: C04 (caché MIR: registro de identidad + huellas interfaz/implementación).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Añadir `MirCacheRecord` (MirFunctionKey + interfaceFingerprint + implementationFingerprint) y su codec. |
| Naturaleza | Añadido (sin cambio de conducta; aún no se consume). |
| Comportamiento idéntico | Sin cambio (codec nuevo). |
| Revisión de referencia | `development` en `cd5e704e8`. |
| Archivos | `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `MirCacheRecord`, `serializeMirCacheRecord`, `restoreMirCacheRecord`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `MirCacheRecord` une la clave observable del callable con las huellas de interfaz e implementación;
  una modificación privada sólo cambia la huella de implementación (invalidación granular). El flujo
  de nodos MIR (implementation fingerprint) es la capa siguiente.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
