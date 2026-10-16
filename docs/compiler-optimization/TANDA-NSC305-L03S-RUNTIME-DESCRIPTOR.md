# Tanda NSC305-L03s — Extraer `computeRuntimeDescriptorAccessorTypes`

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L03 (extraer metadata: cierre transitivo de descriptores runtime → función libre).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Mover el cierre transitivo de tipos con descriptor runtime a `computeRuntimeDescriptorAccessorTypes`. |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `e4f3762eb`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `computeRuntimeDescriptorAccessorTypes`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- El cierre de tipos con descriptor runtime (los importados + sus descendientes) sale de
  `emitDispatchMetadata` a una función libre. Decimonoveno componente de la tanda L03.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
