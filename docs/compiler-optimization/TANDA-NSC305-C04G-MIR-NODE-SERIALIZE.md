# Tanda NSC305-C04g — `serializeMirNodeTree` (serialización del cuerpo MIR)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: C04 (caché MIR: payload relocatable del cuerpo MIR).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Añadir el codec relocatable del árbol de nodos MIR (escalares + strings + flags + topología). |
| Naturaleza | Añadido (sin cambio de conducta; aún no se consume). |
| Comportamiento idéntico | Sin cambio (función nueva). |
| Revisión de referencia | `development` en `954296292`. |
| Archivos | `src/Frontend/ProjectOopMirModel.cpp`. |
| Símbolos | `appendMirU32/U64/String/StringVector`, `MirByteReader`, `appendMirNodeFields`, `serializeMirNodeTree`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `serializeMirNodeTree` recorre el árbol de nodos de forma iterativa (pre-orden) y serializa los
  campos escalares, strings, flags booleanos empaquetados y la topología. Es el payload de la capa C4;
  el restore (deserializar + re-enlazar) es el siguiente paso.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
