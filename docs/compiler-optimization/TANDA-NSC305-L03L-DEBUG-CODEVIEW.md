# Tanda NSC305-L03l — Extraer `debugCodeViewFileNumber` (número CodeView de archivo)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L03 (extraer debug: CodeView file number → `debugCodeViewFileNumberFor`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Mover la resolución del número CodeView de archivo a una función libre. |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `cd96f3eee`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `debugCodeViewFileNumberFor`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `debugCodeViewFileNumber` delega en `debugCodeViewFileNumberFor` (DIBuilder + source manager +
  prefix maps + cachés). Duodécimo componente de la tanda L03.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
