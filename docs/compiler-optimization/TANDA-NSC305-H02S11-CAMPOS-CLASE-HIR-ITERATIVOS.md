# NSC 3.0.5 — H02S11: receptores de campos de clase iterativos

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Eliminar la reentrada HIR en cadenas de campos ordinarios de clase. |
| Naturaleza | Refactor no funcional mediante frames iterativos. |
| Productor | `ProjectOopHirModel::lowerExpression`. |
| Incluido | `ExplicitObjectField` con receiver de clase no nulo y ordinal resuelto. |
| Excluido | Campos implícitos/this/base, structs, accessors, core, extensiones, índices y calls. |
| Contratos | Orden del receiver, FieldValue/FieldAddress, ordinal, volatile, base y errores. |
| Terminación | Rojo/verde a 32,768 niveles, regresión HIR, build de `nsc` y tracking. |

## Cadena causal

Una cadena `this.Next.Next...` llega por `lowerExpressionImpl(MemberExpression)`, llama
`lowerReceiver` y éste vuelve a entrar en `lowerExpression` para el receiver. Cada campo consume un
frame del proceso aunque el HIR resultante es una cadena postorden que puede construirse mediante la
máquina iterativa existente.

## Presupuesto de cambio

- Repositorio: sólo `A:\nsc-devel` sobre `development`.
- Producción: `src/Frontend/HIR/HirExpressionMachine.cpp`.
- Prueba: `tests/unit/ProjectOopLoweringTests.cpp`.
- Archivos protegidos: parser, Sema, MIR, LLVM, ABI, NSMETA, runtime y consumidores.
- Configuración: focal O0; sin baseline, batería completa, deploy, instalación, push, pull o fetch.

## Invariantes

- La clasificación iterativa ocurre después de excluir bindings core, accessors, list-pattern,
  extensiones y valores especiales de lambda.
- Un candidato vacío o `receiverTypeKey` vacío conserva la ruta anterior y su primer error exacto.
- El receiver se baja en contexto de valor antes de construir el campo exterior.
- Sólo el campo exterior usa `FieldAddress` cuando el llamador solicita dirección; los receivers de
  clase interiores permanecen como `FieldValue`.
- `memberOrdinal`, `volatileAccess`, `baseReceiver`, `typeKey` y provenance no cambian.
- El primer fallo del receiver detiene la reconstrucción sin crear el nodo exterior.

## Evidencia

- Contrafactual rojo contra `57dc0442d`: 32,768 campos de clase anidados terminaron con
  `0xC00000FD` (`FOCUS_EXIT=-1073741571`).
- Implementación: `InstanceField` se integró en la máquina común de expresiones con un frame que
  conserva sintaxis, contexto de dirección y binding; el receiver de clase se baja en contexto de
  valor y el campo se reconstruye al desenrollar.
- Foco `NSC_PROJECT_OOP_LOWERING_FOCUS=hir-class-field-depth`: `PASS 3/3`.
- Regresión profunda mixta `hir-mixed-depth`: `PASS 2/2`.
- Regresión profunda de conversiones `hir-conversion-depth`: `PASS 6/6`.
- Regresión `nsc_hir_tests`: `PASS 34/34`.
- Regresión `nsc_namespace_function_hir_tests`: `PASS 12/12`.
- Build focal `nsc`: completado; `nsc.exe static host-runtime contract: passed`.
- No se ejecutaron baseline, batería completa, deploy ni instalación.

## Complejidad y residual

La cadena de campos pasa de consumir un frame del proceso por receptor a tiempo lineal con memoria
auxiliar lineal en heap. Permanecen pendientes H02S12–H02S30, enumerados individualmente en el
tracking; este corte no declara cerrado el lowering HIR completo.
