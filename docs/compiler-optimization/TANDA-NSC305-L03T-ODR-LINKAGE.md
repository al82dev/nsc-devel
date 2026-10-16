# Tanda NSC305-L03t — Extraer `applyOdrLinkage` (weak_odr + COMDAT)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L03 (extraer metadata: linkage ODR → `applyOdrLinkage`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Mover la aplicación de weak_odr + COMDAT a `applyOdrLinkage` (función libre). |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `42b0f6102`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `applyOdrLinkage`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- La aplicación de weak_odr/COMDAT sale de `applyOdrDefinition` a `applyOdrLinkage` (módulo + valor).
  Vigésimo componente de la tanda L03; habilita la extracción del loop de descriptores.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
