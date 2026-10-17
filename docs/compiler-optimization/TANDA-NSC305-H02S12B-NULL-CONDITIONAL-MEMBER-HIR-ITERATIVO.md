# NSC 3.0.5 — H02S12B: null-conditional member HIR iterativo

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Eliminar la reentrada HIR de cadenas null-conditional de campos. |
| Productor | `ProjectOopHirModel::lowerExpression`. |
| Incluido | `NullConditionalMemberExpression` con campo explícito único. |
| Excluido | Getters, calls, índices, assignments y address context inválido. |
| Contratos | Evaluación única, flow-nonnull, ordinal, volatile, tipo y orden. |
| Validación | Diferida a la tanda integral por instrucción expresa del usuario. |

## Cambio

La máquina común incorpora `NullConditionalField`. Cada frame desciende primero al receiver en
contexto de valor y reconstruye al desenrollar `FieldValue` cuando el flujo demuestra non-null o
`NullConditionalFieldValue` en caso contrario.

Los casos inválidos o especiales no entran en la máquina: address context, binding ausente, ordinal
no único y accessor getter conservan la ruta anterior y sus diagnósticos exactos.

## Auditoría estática

- El receiver se evalúa una vez y antes del nodo exterior.
- `flowProvenNonNull` se consulta sobre la misma sintaxis de receiver que antes.
- `booleanValue`, `memberOrdinal`, `volatileAccess`, tipo y provenance mantienen sus fuentes.
- La reconstrucción interior→exterior conserva el orden de una cadena null-conditional.
- No se modificaron parser, Sema, MIR, LLVM, ABI, NSMETA ni formatos.

## Validación diferida

No se compiló ni se ejecutaron pruebas. La validación se acumula para la tanda integral solicitada
por el usuario después de completar toda la implementación pendiente.
