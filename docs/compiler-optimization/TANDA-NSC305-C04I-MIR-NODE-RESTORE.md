# Tanda NSC305-C04i — `restoreMirNodeTree` (deserializar el cuerpo MIR)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: C04 (caché MIR: restore simétrico del árbol de nodos).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Añadir `readMirNodeFields` + `readMirNode` + `restoreMirNodeTree` (deserialización simétrica). |
| Naturaleza | Añadido (sin cambio de conducta; aún no se consume). |
| Comportamiento idéntico | Sin cambio. |
| Revisión de referencia | `development` en `f896d41a6`. |
| Archivos | `src/Frontend/ProjectOopMirModel.cpp`. |
| Símbolos | `readMirNodeFields`, `readMirNode`, `restoreMirNodeTree`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `restoreMirNodeTree` deserializa el árbol (escalares/strings/flags/optionals/vectors/hijos) de forma
  simétrica al `serializeMirNodeTree`; el `syntax` queda a `nullptr` para que el consumidor lo
  re-enlace. Cierra el round-trip del payload C4.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
