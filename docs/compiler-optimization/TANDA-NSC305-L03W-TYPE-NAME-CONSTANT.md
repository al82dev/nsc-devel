# Tanda NSC305-L03w — Extraer `materializeTypeNameConstant` (constante de nombre de tipo)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L03 (extraer metadata: constante de nombre de tipo → función libre).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Mover la materialización de la constante de nombre de tipo a `materializeTypeNameConstant`. |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `099973d77`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `materializeTypeNameConstant`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `internTypeNameConstant` delega en `materializeTypeNameConstant` (módulo + contexto + caché).
  Vigésimo tercer componente de la tanda L03; habilita la extracción del descriptor.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
