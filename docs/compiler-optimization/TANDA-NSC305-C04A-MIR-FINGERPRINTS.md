# Tanda NSC305-C04a — Interface/Implementation fingerprints

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: C04 (caché MIR: separar huellas de interfaz e implementación).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Añadir `InterfaceFingerprint`/`ImplementationFingerprint` + claves SHA-256 separadas. |
| Naturaleza | Añadido (sin cambio de conducta; aún no se consume). |
| Comportamiento idéntico | Sin cambio (helpers nuevos). |
| Revisión de referencia | `development` en `6e79b60f2`. |
| Archivos | `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `InterfaceFingerprint`, `ImplementationFingerprint`, `interfaceFingerprintKey`, `implementationFingerprintKey`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- Estructuras de huella (interfaz: firma/layout/deps públicas; implementación: cuerpo normalizado +
  interfaces usadas) + claves SHA-256 canónicas.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
