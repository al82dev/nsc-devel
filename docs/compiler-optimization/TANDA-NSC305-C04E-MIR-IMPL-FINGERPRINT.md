# Tanda NSC305-C04e — `mirImplementationFingerprint` (flujo de nodos MIR)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: C04 (caché MIR: huella de implementación desde el flujo de nodos).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Añadir `normalizeMirNodeStream` + `mirImplementationFingerprint` (flujo de nodos MIR → SHA-256). |
| Naturaleza | Añadido (sin cambio de conducta; aún no se consume en el cache). |
| Comportamiento idéntico | Sin cambio (función nueva). |
| Revisión de referencia | `development` en `699850cbf`. |
| Archivos | `src/Frontend/ProjectOopMirModel.cpp`. |
| Símbolos | `normalizeMirNodeStream`, `mirImplementationFingerprint`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `normalizeMirNodeStream` produce el flujo normalizado (kind + typeKey + stringValue + calleeName +
  topología) de forma iterativa; `mirImplementationFingerprint` lo hashea (SHA-256). Una modificación
  privada del cuerpo mueve la huella de implementación sin invalidar la interfaz.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
