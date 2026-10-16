# Tanda NSC305-C04b — `mirInterfaceFingerprintKey` (huella de interfaz MIR)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: C04 (caché MIR: huella de interfaz desde los campos observables del callable).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Añadir `mirInterfaceFingerprintKey` (firma observable desde mangledName + tipos). |
| Naturaleza | Añadido (sin cambio de conducta; aún no se consume). |
| Comportamiento idéntico | Sin cambio (helper nuevo). |
| Revisión de referencia | `development` en `fb55ebe92`. |
| Archivos | `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `mirInterfaceFingerprintKey`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `mirInterfaceFingerprintKey` construye la firma de interfaz (nombre + parámetros + return + throws) y
  la clave SHA-256. La huella de implementación (cuerpo MIR) la deriva el consumidor.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
